#include "pmu.h"
#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>

// =============================================================================
// Module PMU -- AXP2101 (gestion d'alimentation T-Beam Q410)
//
// Gere les rails d'alimentation du SX1276 (ALDO2) et du GPS (ALDO3).
// Doit etre initialise en premier : il demarre aussi le bus I2C partage avec BMP280.
// =============================================================================

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
    s_pmu.enableALDO2();   // Rail SX1276 (LoRa)
    s_pmu.setALDO3Voltage(3300);
    s_pmu.enableALDO3();   // Rail module GPS
    delay(200);
    s_ready = true;
    Serial.println("[PMU] AXP2101 initialise (ALDO2=LoRa 3.3V, ALDO3=GPS 3.3V).");
}

float pmuBatteryVoltage() {
    if (!s_ready) return 0.0f;
    return s_pmu.getBattVoltage() / 1000.0f;
}

uint8_t pmuBatteryPercent() {
    if (!s_ready) return 0;
    int pct = s_pmu.getBatteryPercent();
    return (pct < 0) ? 0 : (uint8_t)pct;
}

bool pmuIsCharging() {
    if (!s_ready) return false;
    return s_pmu.isCharging();
}
