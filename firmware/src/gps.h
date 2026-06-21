#pragma once
#include <stdint.h>

struct GpsReading {
  double  latitude;
  double  longitude;
  double  altitudeM;
  double  speedKmh;
  double  hdop;
  uint8_t satellites;
  bool    hasLocation;
  bool    hasAltitude;
  bool    hasSpeed;
};

void       gpsSetup();
GpsReading gpsRead();
