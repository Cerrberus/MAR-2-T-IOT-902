#include "pmu.h"
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

// Module PMU — AXP2101 (T-Beam Q410)
// Gère les rails d'alimentation du SX1276 (ALDO2) et du GPS (ALDO3).
// Démarre aussi Wire, bus I2C partagé avec le BMP280.

static const uint8_t SDA_PIN = 21;
static const uint8_t SCL_PIN = 22;

static XPowersAXP2101 s_pmu;
static bool           s_ready = false;

void pmuSetup() {
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!s_pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, SDA_PIN, SCL_PIN)) {
        Serial.println("[PMU] AXP2101 introuvable — brancher la batterie ou alimenter par USB.");
        s_ready = false;
        return;
    }
    s_pmu.setALDO2Voltage(3300);
    s_pmu.enableALDO2();   // Rail SX1276 (LoRa) — 3.3 V
    s_pmu.setALDO3Voltage(3300);
    s_pmu.enableALDO3();   // Rail module GPS — 3.3 V

    // Délai pour laisser les rails se stabiliser avant l'init LoRa/GPS
    delay(200);

    s_ready = true;
    Serial.println("[PMU] AXP2101 initialise (ALDO2=LoRa 3.3V, ALDO3=GPS 3.3V).");
}

float pmuBatteryVoltage() {
    if (!s_ready) return 0.0f;
    return s_pmu.getBattVoltage() / 1000.0f; // mV → V
}

uint8_t pmuBatteryPercent() {
    if (!s_ready) return 0;
    int pct = s_pmu.getBatteryPercent();
    return (pct < 0) ? 0 : (uint8_t)pct; // getBatteryPercent() retourne -1 si indisponible
}

bool pmuIsCharging() {
    if (!s_ready) return false;
    return s_pmu.isCharging();
}
