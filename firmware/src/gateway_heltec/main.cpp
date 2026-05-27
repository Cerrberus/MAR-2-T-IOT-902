/**
 * GATEWAY — Heltec WiFi LoRa 32 V3
 *
 * Reçoit les paquets JSON du T-Beam via LoRa et les retransmet
 * à l'API backend via WiFi (HTTP POST).
 *
 * Configurer les identifiants WiFi et l'URL de l'API dans platformio.ini
 * (build_flags de l'env heltec_gateway).
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// =============================================================================
// Module LoRa -- SX1262 868 MHz (Heltec WiFi LoRa 32 V3)
// =============================================================================

static const uint8_t PROJECT_LORA_SCK_PIN  = 9;
static const uint8_t PROJECT_LORA_MISO_PIN = 11;
static const uint8_t PROJECT_LORA_MOSI_PIN = 10;
static const uint8_t PROJECT_LORA_NSS_PIN  = 8;
static const uint8_t PROJECT_LORA_RST_PIN  = 12;
static const uint8_t PROJECT_LORA_BUSY_PIN = 13;
static const uint8_t PROJECT_LORA_DIO1_PIN = 14;

static const float   PROJECT_LORA_FREQ_MHZ = 868.0f;
static const float   PROJECT_LORA_BW_KHZ   = 125.0f;
static const uint8_t PROJECT_LORA_SF       = 7;
static const uint8_t PROJECT_LORA_CR       = 5;

static SX1262 g_radio = new Module(PROJECT_LORA_NSS_PIN, PROJECT_LORA_DIO1_PIN,
                                   PROJECT_LORA_RST_PIN, PROJECT_LORA_BUSY_PIN);

static void projectLoraSetup() {
    SPI.begin(PROJECT_LORA_SCK_PIN, PROJECT_LORA_MISO_PIN, PROJECT_LORA_MOSI_PIN);
    int state = g_radio.begin(PROJECT_LORA_FREQ_MHZ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] Echec init (code %d) — arret.\n", state);
        while (true) delay(1000);
    }
    g_radio.setBandwidth(PROJECT_LORA_BW_KHZ);
    g_radio.setSpreadingFactor(PROJECT_LORA_SF);
    g_radio.setCodingRate(PROJECT_LORA_CR);
    g_radio.setDio2AsRfSwitch(true);
    Serial.printf("[LoRa] SX1262 pret (%.1f MHz | SF%d | BW%.0f kHz | CR4/%d)\n",
                  PROJECT_LORA_FREQ_MHZ, PROJECT_LORA_SF, PROJECT_LORA_BW_KHZ, PROJECT_LORA_CR);
}

// =============================================================================
// Module WiFi
// =============================================================================

// Credentials définis dans platformio.ini via build_flags
#ifndef GATEWAY_WIFI_SSID
  #define GATEWAY_WIFI_SSID "votre_reseau"
#endif
#ifndef GATEWAY_WIFI_PASSWORD
  #define GATEWAY_WIFI_PASSWORD "votre_mot_de_passe"
#endif

static void projectWifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return;
    Serial.printf("[WiFi] Connexion a %s...", GATEWAY_WIFI_SSID);
    WiFi.begin(GATEWAY_WIFI_SSID, GATEWAY_WIFI_PASSWORD);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connecte — IP : %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Echec de connexion.");
    }
}

// =============================================================================
// Module NTP — horodatage ISO-8601 pour l'API
// =============================================================================

static void projectNtpSetup() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("[NTP] Synchronisation");
    struct tm timeinfo;
    uint32_t start = millis();
    while (!getLocalTime(&timeinfo, 500) && millis() - start < 10000) {
        Serial.print(".");
    }
    Serial.println();
    if (getLocalTime(&timeinfo, 100)) {
        char buf[30];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        Serial.printf("[NTP] Heure synchronisee : %s\n", buf);
    } else {
        Serial.println("[NTP] Echec — timestamps seront invalides.");
    }
}

static void projectGetTimestamp(char *buf, size_t len) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        snprintf(buf, len, "1970-01-01T00:00:00Z");
        return;
    }
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

// =============================================================================
// Module API — POST vers le backend
// =============================================================================

#ifndef GATEWAY_API_URL
  #define GATEWAY_API_URL "http://localhost:8000/api/v1/measurements"
#endif
#ifndef GATEWAY_API_TOKEN
  #define GATEWAY_API_TOKEN "dev-token"
#endif

static uint32_t g_rxCount  = 0;
static uint32_t g_okCount  = 0;

static void projectApiPost(const String &loraJson, float rssi, float snr) {
    // Reconnexion WiFi si nécessaire
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Deconnecte — tentative de reconnexion...");
        projectWifiConnect();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[API] Abandon : pas de WiFi.");
            return;
        }
    }

    // Parse le JSON reçu du T-Beam
    JsonDocument src;
    DeserializationError err = deserializeJson(src, loraJson);
    if (err) {
        Serial.printf("[API] JSON invalide : %s\n", err.c_str());
        return;
    }

    const char *deviceId = src["id"]  | "";
    const char *fw       = src["fw"]  | "0.0.0";
    const char *msgId    = src["msg"] | "";
    float lat = src["lat"] | 0.0f;
    float lng = src["lng"] | 0.0f;

    // Données capteurs (optionnelles selon ce que le T-Beam a pu mesurer)
    bool  hasDust = src.containsKey("pm25");
    float pm25    = src["pm25"] | 0.0f;

    bool  hasBmp  = src.containsKey("temp") && src.containsKey("pres");
    float temp    = src["temp"] | 0.0f;
    float pres    = src["pres"] | 0.0f;

    float battV = src["batt_v"] | 0.0f;
    int   battP = src["batt_p"] | 0;
    bool  battC = src["batt_c"] | false;

    char timestamp[30];
    projectGetTimestamp(timestamp, sizeof(timestamp));

    // Construit le payload attendu par l'API
    JsonDocument payload;
    payload["message_id"]                      = msgId;
    payload["device"]["id"]                    = deviceId;
    payload["device"]["firmware_version"]      = fw;
    payload["device"]["location"]["latitude"]  = lat;
    payload["device"]["location"]["longitude"] = lng;
    payload["timestamp"]                       = timestamp;
    payload["transmission"]["protocol"]        = "LoRa";
    payload["transmission"]["rssi"]            = (int)rssi;
    payload["transmission"]["snr"]             = snr;
    payload["battery"]["voltage_v"]            = battV;
    payload["battery"]["percentage"]           = battP;
    payload["battery"]["charging"]             = battC;

    if (hasDust) {
        payload["sensors"]["dust"]["type"] = "GP2Y1010AU0F";
        payload["sensors"]["dust"]["P1"]   = pm25;  // estimation PM10
        payload["sensors"]["dust"]["P2"]   = pm25;  // estimation PM2.5
    }
    if (hasBmp) {
        payload["sensors"]["bme280"]["type"]        = "BMP280";
        payload["sensors"]["bme280"]["temperature"] = temp;
        payload["sensors"]["bme280"]["pressure"]    = pres;
    }

    String body;
    serializeJson(payload, body);

    HTTPClient http;
    http.begin(GATEWAY_API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " GATEWAY_API_TOKEN);
    int code = http.POST(body);

    if (code == 201) {
        g_okCount++;
        Serial.printf("[API] OK (#%lu) %s\n", g_okCount, msgId);
    } else if (code == 409) {
        Serial.printf("[API] Doublon ignore (%s)\n", msgId);
    } else if (code > 0) {
        Serial.printf("[API] Erreur HTTP %d : %s\n", code, http.getString().c_str());
    } else {
        Serial.printf("[API] Erreur reseau : %s\n", http.errorToString(code).c_str());
    }
    http.end();
}

// =============================================================================
// Setup / Loop
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("[Gateway] Heltec WiFi LoRa 32 V3 — demarrage");

    projectWifiConnect();
    projectNtpSetup();
    projectLoraSetup();

    Serial.println("[Gateway] En attente de paquets LoRa du T-Beam...");
    Serial.println();
}

void loop() {
    String received;
    int state = g_radio.receive(received);

    if (state == RADIOLIB_ERR_NONE) {
        g_rxCount++;
        float rssi = g_radio.getRSSI();
        float snr  = g_radio.getSNR();
        Serial.printf("[LoRa] #%lu recu | RSSI: %.1f dBm | SNR: %.1f dB\n",
                      g_rxCount, rssi, snr);
        Serial.printf("       %s\n", received.c_str());
        projectApiPost(received, rssi, snr);
        Serial.printf("       OK envoyes : %lu / %lu\n\n", g_okCount, g_rxCount);

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[LoRa] Paquet corrompu (CRC error)");
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.printf("[LoRa] Erreur RX : %d\n", state);
    }
}
