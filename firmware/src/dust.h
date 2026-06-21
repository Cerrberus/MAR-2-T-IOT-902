#pragma once
#include <Arduino.h>

struct DustReading {
  float      ledOffAvg;    // ADC average with LED off  (ambient baseline)
  float      ledOnAvg;     // ADC average with LED on
  float      rawDelta;     // ledOnAvg - ledOffAvg
  float      smoothedDelta;// rawDelta minus calibration baseline, smoothed, >= 0
  float      pm25UgM3;     // estimated PM2.5 in µg/m³
  uint8_t    levelPct;     // dust index 0-100 %
  const char *label;       // qualitative label
  const char *state;       // module state: warmup / calibration / ready / no_signal
  bool       signalValid;  // true if ADC signal exceeds minimum thresholds
  bool       ready;        // true if reading is usable
};

void        dustSetup();
DustReading dustRead();
void        dustAppendJson(String &json);
float       dustBaselineDelta();
