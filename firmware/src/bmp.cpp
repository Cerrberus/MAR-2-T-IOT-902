#include "bmp.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>

// Module BMP280 — température et pression (I2C)
// Teste 0x76 puis 0x77 : l'adresse dépend de la broche SDO du module.
// Wire est démarré par pmuSetup() (bus I2C partagé avec l'AXP2101).

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

    // Sur-échantillonnage x16 pression pour réduire le bruit, x2 temp suffit.
    // Filtre IIR x16 pour atténuer les variations courtes (claquement de porte, vent).
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
    r.pressureHpa = s_bmp.readPressure() / 100.0f; // Pa → hPa
    r.altitudeM   = s_bmp.readAltitude(1013.25f);
    r.ready       = true;
    return r;
}
