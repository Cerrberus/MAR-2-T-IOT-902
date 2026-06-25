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

static const uint8_t SCK_PIN          = 9;
static const uint8_t MISO_PIN         = 11;
static const uint8_t MOSI_PIN         = 10;
static const uint8_t NSS_PIN          = 8;
static const uint8_t RST_PIN          = 12;
static const uint8_t BUSY_PIN         = 13;
static const uint8_t DIO1_PIN         = 14;

static const float   FREQ_MHZ         = 868.0f;
static const float   BANDWIDTH_KHZ    = 125.0f;
static const uint8_t SPREADING_FACTOR = 7;
static const uint8_t CODING_RATE      = 5;

static SX1262 s_radio = new Module(NSS_PIN, DIO1_PIN, RST_PIN, BUSY_PIN);

static void loraSetup() {
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);
    int state = s_radio.begin(FREQ_MHZ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] Echec init (code %d) — arret.\n", state);
        while (true) delay(1000);
    }
    s_radio.setBandwidth(BANDWIDTH_KHZ);
    s_radio.setSpreadingFactor(SPREADING_FACTOR);
    s_radio.setCodingRate(CODING_RATE);
    s_radio.setDio2AsRfSwitch(true);
    Serial.printf("[LoRa] SX1262 pret (%.1f MHz | SF%d | BW%.0f kHz | CR4/%d)\n",
                  FREQ_MHZ, SPREADING_FACTOR, BANDWIDTH_KHZ, CODING_RATE);
}

// =============================================================================
// Module WiFi
// =============================================================================

// Credentials definis dans platformio.ini via build_flags
#ifndef GATEWAY_WIFI_SSID
  #define GATEWAY_WIFI_SSID "votre_reseau"
#endif
#ifndef GATEWAY_WIFI_PASSWORD
  #define GATEWAY_WIFI_PASSWORD "votre_mot_de_passe"
#endif

static void wifiConnect() {
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
// Module NTP -- horodatage ISO-8601 pour l'API
// =============================================================================

static void ntpSetup() {
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

static void getTimestamp(char *buf, size_t len) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        snprintf(buf, len, "1970-01-01T00:00:00Z");
        return;
    }
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

// =============================================================================
// Module API -- POST vers le backend
// =============================================================================

#ifndef GATEWAY_API_URL
  #define GATEWAY_API_URL "http://localhost:8000/api/v1/measurements"
#endif
#ifndef GATEWAY_API_TOKEN
  #define GATEWAY_API_TOKEN "dev-token"
#endif

static uint32_t s_rxCount = 0;
static uint32_t s_okCount = 0;

static void apiPost(const String &loraJson, float rssi, float snr) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Deconnecte — tentative de reconnexion...");
        wifiConnect();
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[API] Abandon : pas de WiFi.");
            return;
        }
    }

    JsonDocument src;
    DeserializationError err = deserializeJson(src, loraJson);
    if (err) {
        Serial.printf("[API] JSON invalide : %s\n", err.c_str());
        return;
    }

    const char *deviceId = src["id"]  | "";
    const char *fw       = src["fw"]  | "0.0.0";
    const char *msgId    = src["msg"] | "";
    float latitude  = src["lat"] | 0.0f;
    float longitude = src["lng"] | 0.0f;

    bool  hasDust   = src.containsKey("pm25");
    float pm25      = src["pm25"] | 0.0f;
    int   dustIdx   = src["dust"] | -1;

    bool  hasBmp    = src.containsKey("temp") && src.containsKey("pres");
    float tempC     = src["temp"] | 0.0f;
    float pressHpa  = src["pres"] | 0.0f;

    float battVoltage  = src["batt_v"] | 0.0f;
    int   battPercent  = src["batt_p"] | 0;
    bool  battCharging = src["batt_c"] | false;

    char timestamp[30];
    getTimestamp(timestamp, sizeof(timestamp));

    JsonDocument payload;
    payload["message_id"]                      = msgId;
    payload["device"]["id"]                    = deviceId;
    payload["device"]["firmware_version"]      = fw;
    payload["device"]["location"]["latitude"]  = latitude;
    payload["device"]["location"]["longitude"] = longitude;
    payload["timestamp"]                       = timestamp;
    payload["transmission"]["protocol"]        = "LoRa";
    payload["transmission"]["rssi"]            = (int)rssi;
    payload["transmission"]["snr"]             = snr;
    payload["battery"]["voltage_v"]            = battVoltage;
    payload["battery"]["percentage"]           = battPercent;
    payload["battery"]["charging"]             = battCharging;

    if (hasDust) {
        payload["sensors"]["dust"]["type"] = "GP2Y1010AU0F";
        payload["sensors"]["dust"]["P1"]   = nullptr;
        payload["sensors"]["dust"]["P2"]   = pm25;
        if (dustIdx >= 0)
            payload["sensors"]["dust"]["index_pct"] = dustIdx;
    }
    if (hasBmp) {
        payload["sensors"]["bme280"]["type"]        = "BMP280";
        payload["sensors"]["bme280"]["temperature"] = tempC;
        payload["sensors"]["bme280"]["pressure"]    = pressHpa;
    }

    String body;
    serializeJson(payload, body);

    HTTPClient http;
    http.begin(GATEWAY_API_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " GATEWAY_API_TOKEN);
    int code = http.POST(body);

    if (code == 201) {
        s_okCount++;
        Serial.printf("[API] OK (#%lu) %s\n", s_okCount, msgId);
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

    wifiConnect();
    ntpSetup();
    loraSetup();

    Serial.println("[Gateway] En attente de paquets LoRa du T-Beam...");
    Serial.println();
}

void loop() {
    String received;
    int state = s_radio.receive(received);

    if (state == RADIOLIB_ERR_NONE) {
        s_rxCount++;
        float rssi = s_radio.getRSSI();
        float snr  = s_radio.getSNR();
        Serial.printf("[LoRa] #%lu recu | RSSI: %.1f dBm | SNR: %.1f dB\n",
                      s_rxCount, rssi, snr);
        Serial.printf("       %s\n", received.c_str());
        apiPost(received, rssi, snr);
        Serial.printf("       OK envoyes : %lu / %lu\n\n", s_okCount, s_rxCount);

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("[LoRa] Paquet corrompu (CRC error)");
    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.printf("[LoRa] Erreur RX : %d\n", state);
    }
}
