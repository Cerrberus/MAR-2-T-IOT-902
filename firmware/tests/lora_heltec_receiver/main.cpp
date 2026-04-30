/**
 * TEST — Récepteur LoRa sur Heltec WiFi LoRa 32 V3
 * Affiche chaque paquet reçu avec RSSI et SNR.
 *
 * Puce LoRa : SX1262 (868 MHz)
 * Émetteur attendu : LILYGO T-Beam V1.2 (SX1276)
 *
 * Flasher  : pio run -e lora_heltec_receiver_test --target upload
 * Monitor  : pio device monitor -e lora_heltec_receiver_test
 */

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ─── Pins SPI LoRa (Heltec WiFi LoRa 32 V3) ──────────────────────────────────
#define LORA_SCK   9
#define LORA_MISO  11
#define LORA_MOSI  10
#define LORA_NSS   8
#define LORA_RST   12
#define LORA_BUSY  13
#define LORA_DIO1  14

// ─── Paramètres LoRa (doivent correspondre à l'émetteur) ─────────────────────
#define LORA_FREQ   868.0   // MHz
#define LORA_BW     125.0   // kHz
#define LORA_SF     7
#define LORA_CR     5

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

int rxCount = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║  TEST LoRa — Heltec V3 (récepteur)        ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    Serial.println();

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    Serial.print("  SX1262 init... ");
    int state = radio.begin(LORA_FREQ);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
    } else {
        Serial.printf("[!] Echec (code %d) — vérifier antenne et branchements\n", state);
        while (true) delay(1000);
    }

    radio.setBandwidth(LORA_BW);
    radio.setSpreadingFactor(LORA_SF);
    radio.setCodingRate(LORA_CR);

    // Le Heltec V3 utilise DIO2 comme commutateur RF
    radio.setDio2AsRfSwitch(true);

    Serial.printf("  Fréquence : %.1f MHz | BW : %.0f kHz | SF%d | CR4/%d\n",
        LORA_FREQ, LORA_BW, LORA_SF, LORA_CR);
    Serial.println();
    Serial.println("  En attente de paquets du T-Beam...");
    Serial.println("  ──────────────────────────────────────────────────────");
    Serial.println("  #      | Message              | RSSI (dBm) | SNR (dB)");
    Serial.println("  ──────────────────────────────────────────────────────");
}

void loop() {
    String received;
    int state = radio.receive(received);

    if (state == RADIOLIB_ERR_NONE) {
        rxCount++;
        Serial.printf("  %6d | %-20s | %8.1f   | %6.1f\n",
            rxCount,
            received.c_str(),
            radio.getRSSI(),
            radio.getSNR());

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("  [!] Paquet reçu avec erreur CRC");

    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.printf("  [!] Erreur RX : %d\n", state);
    }
}
