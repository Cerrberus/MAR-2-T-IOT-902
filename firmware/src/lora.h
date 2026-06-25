#pragma once
#include <Arduino.h>

// Module LoRa — SX1276 868 MHz (RadioLib)
// Le rail ALDO2 (PMU) doit être actif avant loraSetup().
void loraSetup();
bool loraSend(String payload); // true si envoyé, false si radio non prête ou erreur TX
