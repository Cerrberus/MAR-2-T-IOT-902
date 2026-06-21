#pragma once

struct BmpReading {
  float tempC;
  float pressureHpa;
  float altitudeM;
  bool  ready;
};

void       bmpSetup();
BmpReading bmpRead();
