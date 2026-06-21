#include "lora.h"
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// =============================================================================
// Module LoRa -- SX1276 868 MHz (RadioLib)
//
// Brochage SPI specifique au T-Beam V1.2.
// SF7 : bon compromis portee/debit. Puissance 10 dBm (limite legale EU).
// Le rail ALDO2 doit etre active par le PMU avant cet init.
// =============================================================================

static const uint8_t SCK_PIN         = 5;
static const uint8_t MISO_PIN        = 19;
static const uint8_t MOSI_PIN        = 27;
static const uint8_t NSS_PIN         = 18;
static const uint8_t RST_PIN         = 23;
static const uint8_t DIO0_PIN        = 26;
static const uint8_t DIO1_PIN        = 33;

static const float   FREQ_MHZ        = 868.0f;
static const float   BANDWIDTH_KHZ   = 125.0f;
static const uint8_t SPREADING_FACTOR = 7;
static const uint8_t CODING_RATE     = 5;
static const int8_t  TX_POWER_DBM    = 10;

static SX1276 s_radio = new Module(NSS_PIN, DIO0_PIN, RST_PIN, DIO1_PIN);
static bool   s_ready = false;

void loraSetup() {
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

    int state = s_radio.begin(FREQ_MHZ);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] Initialisation echouee (code %d)\n", state);
        s_ready = false;
        return;
    }
    s_radio.setOutputPower(TX_POWER_DBM);
    s_radio.setBandwidth(BANDWIDTH_KHZ);
    s_radio.setSpreadingFactor(SPREADING_FACTOR);
    s_radio.setCodingRate(CODING_RATE);
    s_ready = true;
    Serial.printf("[LoRa] SX1276 initialise (%.1f MHz | SF%d | BW%.0f kHz | CR4/%d | %d dBm)\n",
                  FREQ_MHZ, SPREADING_FACTOR, BANDWIDTH_KHZ, CODING_RATE, TX_POWER_DBM);
}

bool loraSend(String payload) {
    if (!s_ready) return false;
    return s_radio.transmit(payload.c_str()) == RADIOLIB_ERR_NONE;
}
