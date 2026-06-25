#include <Arduino.h>
#include "pmu.h"
#include "lora.h"
#include "dust.h"
#include "bmp.h"
#include "gps.h"

#define FIRMWARE_VERSION "1.0.0"

// Généré au démarrage depuis l'adresse MAC eFuse — unique par puce, sans réseau
static char g_deviceId[32];

// Construit le payload JSON à envoyer par LoRa.
// Les champs GPS / BMP / dust / batterie sont omis si le capteur n'est pas prêt.
static String buildPayload(
    GpsReading  gps,
    BmpReading  bmp,
    DustReading dust,
    float       battVoltage,
    uint8_t     battPercent,
    bool        battCharging,
    uint32_t    seqNum)
{
    char msgId[64];
    snprintf(msgId, sizeof(msgId), "%s-%lu", g_deviceId, (unsigned long)millis());

    String json = "{";
    json += "\"id\":\""   + String(g_deviceId) + "\"";
    json += ",\"fw\":\""  + String(FIRMWARE_VERSION) + "\"";
    json += ",\"msg\":\"" + String(msgId) + "\"";
    json += ",\"seq\":"   + String(seqNum);

    json += ",\"gps_ok\":" + String(gps.hasLocation ? "true" : "false");
    json += ",\"sats\":"   + String(gps.satellites);
    if (gps.hasLocation) {
        json += ",\"lat\":"  + String(gps.latitude,  6);
        json += ",\"lng\":"  + String(gps.longitude, 6);
    }
    if (gps.hasAltitude) json += ",\"alt\":" + String(gps.altitudeM, 1);

    if (bmp.ready) {
        json += ",\"temp\":"  + String(bmp.tempC,       2);
        json += ",\"pres\":"  + String(bmp.pressureHpa, 2);
    }

    if (dust.ready) {
        json += ",\"pm25\":"  + String(dust.pm25UgM3, 1);
        json += ",\"dust\":"  + String((unsigned int)dust.levelPct);
    }

    // battVoltage == 0 si le PMU est absent (alimentation USB directe)
    if (battVoltage > 0.0f) {
        json += ",\"batt_v\":"  + String(battVoltage, 2);
        json += ",\"batt_p\":"  + String((unsigned int)battPercent);
        json += ",\"batt_c\":"  + String(battCharging ? "true" : "false");
    }

    json += "}";
    return json;
}

// Barre de progression ASCII sur 20 caractères pour le moniteur série
static void printBar(uint8_t pct) {
    const uint8_t width  = 20;
    uint8_t       filled = (pct * width + 50) / 100;
    Serial.print("[");
    for (uint8_t i = 0; i < width; i++) {
        Serial.print(i < filled ? "#" : "-");
    }
    Serial.print("]");
}

// Dérive l'ID appareil depuis l'adresse MAC eFuse (6 octets, unique par puce)
static void initDeviceId() {
    uint64_t mac = ESP.getEfuseMac();
    uint8_t  b[6];
    b[0] = (mac >> 40) & 0xFF;
    b[1] = (mac >> 32) & 0xFF;
    b[2] = (mac >> 24) & 0xFF;
    b[3] = (mac >> 16) & 0xFF;
    b[4] = (mac >>  8) & 0xFF;
    b[5] =  mac        & 0xFF;
    snprintf(g_deviceId, sizeof(g_deviceId),
             "esp32-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5]);
}

void setup() {
    Serial.begin(115200);
    initDeviceId();

    Serial.printf("\n[Boot] Device ID : %s\n", g_deviceId);
    Serial.printf("[Boot] Firmware  : %s\n\n", FIRMWARE_VERSION);

    // PMU en premier : démarre le bus I2C et active les rails ALDO2 (LoRa) + ALDO3 (GPS)
    pmuSetup();

    dustSetup();
    bmpSetup();
    gpsSetup();
    loraSetup();
}

void loop() {
    static uint32_t s_seqNum = 0;
    s_seqNum++;

    const DustReading dust    = dustRead();
    const BmpReading  bmp     = bmpRead();
    const GpsReading  gps     = gpsRead();
    const float       battV   = pmuBatteryVoltage();
    const uint8_t     battPct = pmuBatteryPercent();
    const bool        battChg = pmuIsCharging();

    Serial.println("========================================");
    Serial.printf("  Device : %s   t = %lu ms\n", g_deviceId, millis());
    Serial.println("========================================");

    Serial.println("  [GPS]");
    Serial.printf("  Satellites : %u   HDOP: %.1f\n", gps.satellites, gps.hdop);
    if (gps.hasLocation) {
        Serial.printf("  Position   : %.6f, %.6f\n", gps.latitude, gps.longitude);
    } else {
        Serial.println("  Position   : (pas de fix)");
    }
    if (gps.hasAltitude) Serial.printf("  Altitude   : %.1f m\n",   gps.altitudeM);
    if (gps.hasSpeed)    Serial.printf("  Vitesse    : %.1f km/h\n", gps.speedKmh);
    Serial.println();

    Serial.println("  [BMP280]");
    if (bmp.ready) {
        Serial.printf("  Temp     : %.2f degC\n",  bmp.tempC);
        Serial.printf("  Pression : %.2f hPa\n",   bmp.pressureHpa);
        Serial.printf("  Altitude : %.1f m\n",      bmp.altitudeM);
    } else {
        Serial.println("  (capteur absent ou non initialise)");
    }
    Serial.println();

    Serial.printf("  [Dust]  state: %s\n", dust.state);
    Serial.printf("  LED off  : %7.2f ADC\n", dust.ledOffAvg);
    Serial.printf("  LED on   : %7.2f ADC\n", dust.ledOnAvg);
    Serial.printf("  Delta    : %+7.2f ADC  (baseline: %.2f)\n", dust.rawDelta, dustBaselineDelta());
    Serial.printf("  Smoothed : %+7.2f ADC\n", dust.smoothedDelta);
    Serial.println();
    if (dust.ready) {
        Serial.printf("  Level    : %3u%%  %s  ", dust.levelPct, dust.label);
        printBar(dust.levelPct);
        Serial.println();
        Serial.printf("  PM2.5 ~  : %.1f ug/m3\n", dust.pm25UgM3);
    } else {
        Serial.println("  (en attente de donnees valides)");
    }
    Serial.println();

    Serial.println("  [Battery]");
    if (battV > 0.0f) {
        Serial.printf("  %.2f V  %u%%  %s\n", battV, battPct, battChg ? "charging" : "discharging");
    } else {
        Serial.println("  (PMU absent — alimentation USB)");
    }
    Serial.println();

    String payload = buildPayload(gps, bmp, dust, battV, battPct, battChg, s_seqNum);
    bool   sent    = loraSend(payload);

    Serial.println("  [LoRa]");
    Serial.printf("  Payload (%u oct) : %s\n", (unsigned int)payload.length(), payload.c_str());
    Serial.printf("  Sent            : %s\n",  sent ? "OK" : "FAILED");
    Serial.println();

    delay(4000);
}
