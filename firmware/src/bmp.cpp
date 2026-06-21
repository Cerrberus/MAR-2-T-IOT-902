#include "bmp.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// =============================================================================
// Module BMP280 -- temperature et pression atmospherique (I2C)
//
// Teste 0x76 puis 0x77 car la broche SDO varie selon les modules.
// Wire.begin() est appele dans pmuSetup() (bus partage avec AXP2101).
// =============================================================================

static Adafruit_BMP280 s_bmp;
static bool            s_ready = false;

void bmpSetup() {
    uint8_t addr = 0;
    if      (s_bmp.begin(0x76)) { addr = 0x76; }
    else if (s_bmp.begin(0x77)) { addr = 0x77; }
    else {
        Serial.println("[BMP280] Capteur introuvable (0x76 et 0x77) — verifier adresse et cablage.");
        s_ready = false;
        return;
    }

    s_bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,
                      Adafruit_BMP280::SAMPLING_X16,
                      Adafruit_BMP280::FILTER_X16,
                      Adafruit_BMP280::STANDBY_MS_500);
    s_ready = true;
    Serial.printf("[BMP280] Capteur initialise (addr=0x%02X)\n", addr);
}

BmpReading bmpRead() {
    BmpReading r = {};
    if (!s_ready) {
        r.ready = false;
        return r;
    }
    r.tempC       = s_bmp.readTemperature();
    r.pressureHpa = s_bmp.readPressure() / 100.0f;
    r.altitudeM   = s_bmp.readAltitude(1013.25f);
    r.ready       = true;
    return r;
}
