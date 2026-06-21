#pragma once
#include <stdint.h>

void    pmuSetup();
float   pmuBatteryVoltage();
uint8_t pmuBatteryPercent();
bool    pmuIsCharging();
