#pragma once

struct BmpReading {
  float tempC;        // température en °C
  float pressureHpa;  // pression atmosphérique en hPa
  float altitudeM;    // altitude estimée en m (référence 1013.25 hPa)
  bool  ready;        // false si capteur absent ou erreur I2C
};

void       bmpSetup();
BmpReading bmpRead();
