#include "gps.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>

// Module GPS — NEO-6M/M8N via UART1 (HardwareSerial 1)

static const uint8_t  RX_PIN         = 34;   // ESP32 reçoit les trames NMEA
static const uint8_t  TX_PIN         = 12;   // ESP32 envoie des commandes (rarement utilisé)
static const uint32_t BAUD_RATE      = 9600;
static const uint32_t FEED_WINDOW_MS = 400;  // durée de lecture NMEA par cycle (couvre plusieurs trames)

static TinyGPSPlus    s_gps;
static HardwareSerial s_serial(1); // UART1

void gpsSetup() {
    s_serial.begin(BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[GPS] UART initialise (RX=%d, TX=%d, %lu baud)\n",
                  RX_PIN, TX_PIN, BAUD_RATE);
}

GpsReading gpsRead() {
    // Alimente le parseur NMEA pendant FEED_WINDOW_MS pour capter les phrases GGA/RMC
    uint32_t deadline = millis() + FEED_WINDOW_MS;
    while (millis() < deadline) {
        while (s_serial.available()) {
            s_gps.encode(s_serial.read());
        }
    }

    GpsReading r = {};
    // age() < 2000 : rejette les fixes datant de plus de 2 s (données périmées)
    r.hasLocation = s_gps.location.isValid() && s_gps.location.age() < 2000;
    r.hasAltitude = s_gps.altitude.isValid() && s_gps.altitude.age() < 2000;
    r.hasSpeed    = s_gps.speed.isValid()    && s_gps.speed.age()    < 2000;
    r.satellites  = (uint8_t)s_gps.satellites.value();
    r.hdop        = s_gps.hdop.hdop();

    if (r.hasLocation) {
        r.latitude  = s_gps.location.lat();
        r.longitude = s_gps.location.lng();
    }
    if (r.hasAltitude) r.altitudeM = s_gps.altitude.meters();
    if (r.hasSpeed)    r.speedKmh  = s_gps.speed.kmph();

    return r;
}
