#pragma once
#include <stdint.h>

struct GpsReading {
  double  latitude;     // degrés décimaux (+ = Nord)
  double  longitude;    // degrés décimaux (+ = Est)
  double  altitudeM;    // altitude WGS84 en mètres
  double  speedKmh;     // vitesse sol en km/h
  double  hdop;         // précision horizontale (< 1 excellent, > 5 mauvais)
  uint8_t satellites;   // nombre de satellites en vue
  bool    hasLocation;  // lat/lng valides et âge < 2 s
  bool    hasAltitude;
  bool    hasSpeed;
};

void       gpsSetup();
GpsReading gpsRead(); // bloque ~400 ms pour lire les trames NMEA
