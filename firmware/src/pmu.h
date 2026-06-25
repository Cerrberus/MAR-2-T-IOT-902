#pragma once
#include <stdint.h>

// Gestion alimentation AXP2101 (T-Beam Q410).
// pmuSetup() doit être appelé en premier : il démarre le bus I2C partagé
// et active les rails 3.3 V du SX1276 (ALDO2) et du GPS (ALDO3).
void    pmuSetup();
float   pmuBatteryVoltage(); // volts ; 0.0 si PMU absent
uint8_t pmuBatteryPercent(); // 0-100 ; 0 si PMU absent
bool    pmuIsCharging();
