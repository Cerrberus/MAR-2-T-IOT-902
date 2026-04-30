/**
 * TEST — Émetteur LoRa sur LILYGO T-Beam V1.2
 * Envoie un paquet toutes les 3 secondes.
 *
 * Puce LoRa : SX1276 (868 MHz)
 * Récepteur attendu : Heltec WiFi LoRa 32 V3 (SX1262)
 *
 * Flasher  : pio run -e lora_tbeam_sender_test --target upload
 * Monitor  : pio device monitor -e lora_tbeam_sender_test
 *
 * ATTENTION : sur T-Beam V1.2, l'AXP192 doit activer LDO2
 *             pour alimenter le SX1276. Sans ça, radio.begin() échoue.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <XPowersLib.h>

// ─── Pins SPI LoRa (T-Beam V1.2) ─────────────────────────────────────────────
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_NSS   18
#define LORA_RST   23
#define LORA_DIO0  26
#define LORA_DIO1  33

// ─── I2C AXP192 (T-Beam V1.2) ────────────────────────────────────────────────
#define PMU_SDA 21
#define PMU_SCL 22

// ─── Paramètres LoRa ──────────────────────────────────────────────────────────
#define LORA_FREQ   868.0   // MHz — Europe
#define LORA_BW     125.0   // kHz
#define LORA_SF     7       // Spreading Factor
#define LORA_CR     5       // Coding Rate 4/5
#define LORA_POWER  10      // dBm

XPowersAXP192 PMU;
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);

int counter = 0;

bool initPMU() {
    Wire.begin(PMU_SDA, PMU_SCL);
    if (!PMU.begin(Wire, AXP192_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        return false;
    }
    PMU.setLDO2Voltage(3300);
    PMU.enableLDO2();   // alimente le SX1276
    delay(100);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║  TEST LoRa — T-Beam V1.2 (émetteur)  ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.println();

    Serial.print("  AXP192 (alimentation SX1276)... ");
    if (initPMU()) {
        Serial.println("OK — LDO2 3.3V activé");
    } else {
        Serial.println("[!] Echec — LoRa peut ne pas fonctionner");
    }

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI);

    Serial.print("  SX1276 init... ");
    int state = radio.begin(LORA_FREQ);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
    } else {
        Serial.printf("[!] Echec (code %d) — vérifier antenne et branchements\n", state);
        while (true) delay(1000);
    }

    radio.setOutputPower(LORA_POWER);
    radio.setBandwidth(LORA_BW);
    radio.setSpreadingFactor(LORA_SF);
    radio.setCodingRate(LORA_CR);

    Serial.printf("  Fréquence : %.1f MHz | BW : %.0f kHz | SF%d | CR4/%d | %d dBm\n",
        LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_POWER);
    Serial.println();
    Serial.println("  Début émission (1 paquet toutes les 3 s)...");
    Serial.println("  ─────────────────────────────────────────────");
    Serial.println("  #      | Message              | Statut");
    Serial.println("  ─────────────────────────────────────────────");
}

void loop() {
    counter++;
    String msg = "T-Beam#" + String(counter);

    int state = radio.transmit(msg);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("  %6d | %-20s | OK — airtime %.0f ms\n",
            counter, msg.c_str(), radio.getTimeOnAir(msg.length()) / 1000.0);
    } else {
        Serial.printf("  %6d | %-20s | ERREUR %d\n",
            counter, msg.c_str(), state);
    }

    delay(3000);
}
