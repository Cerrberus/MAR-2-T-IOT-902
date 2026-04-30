/**
 * TEST — Récepteur LoRa sur Heltec WiFi LoRa 32 V3
 * Reçoit et affiche les données GPS envoyées par le T-Beam.
 *
 * Format paquet reçu :
 *   GPS fix  : "GPS,lat,lon,alt,sats,#"     ex: "GPS,48.856600,2.352200,35.1,6,12"
 *   Sans fix : "NOGPS,Xsat,#"               ex: "NOGPS,3sat,5"
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

// ─── Paramètres LoRa (identiques à l'émetteur) ───────────────────────────────
#define LORA_FREQ   868.0
#define LORA_BW     125.0
#define LORA_SF     7
#define LORA_CR     5

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

int rxCount  = 0;
int gpsCount = 0;   // paquets avec fix GPS valide

// ─── Affiche un paquet GPS décodé ────────────────────────────────────────────
void printGPS(const String& msg, float rssi, float snr) {
    // Format : GPS,lat,lon,alt,sats,counter
    int p1 = msg.indexOf(',');
    int p2 = msg.indexOf(',', p1 + 1);
    int p3 = msg.indexOf(',', p2 + 1);
    int p4 = msg.indexOf(',', p3 + 1);
    int p5 = msg.indexOf(',', p4 + 1);

    String lat  = msg.substring(p1 + 1, p2);
    String lon  = msg.substring(p2 + 1, p3);
    String alt  = msg.substring(p3 + 1, p4);
    String sats = msg.substring(p4 + 1, p5);
    String num  = msg.substring(p5 + 1);

    gpsCount++;
    Serial.println("  ┌─────────────────────────────────────────┐");
    Serial.printf ("  │ Paquet #%-4s  RSSI: %5.1f dBm  SNR: %4.1f dB │\n",
        num.c_str(), rssi, snr);
    Serial.println("  ├─────────────────────────────────────────┤");
    Serial.printf ("  │  Latitude   : %-28s│\n", (lat  + " °").c_str());
    Serial.printf ("  │  Longitude  : %-28s│\n", (lon  + " °").c_str());
    Serial.printf ("  │  Altitude   : %-28s│\n", (alt  + " m").c_str());
    Serial.printf ("  │  Satellites : %-28s│\n", sats.c_str());
    Serial.println("  └─────────────────────────────────────────┘");
}

// ─── Affiche un paquet sans fix GPS ──────────────────────────────────────────
void printNoGPS(const String& msg, float rssi, float snr) {
    Serial.printf("  [#%d] Pas de fix GPS — %s | RSSI: %.1f dBm | SNR: %.1f dB\n",
        rxCount, msg.c_str(), rssi, snr);
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║  TEST LoRa+GPS — Heltec V3 (récepteur)       ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.println();

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    Serial.print("  SX1262 init... ");
    int state = radio.begin(LORA_FREQ);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
    } else {
        Serial.printf("[!] Echec (code %d)\n", state);
        while (true) delay(1000);
    }

    radio.setBandwidth(LORA_BW);
    radio.setSpreadingFactor(LORA_SF);
    radio.setCodingRate(LORA_CR);
    radio.setDio2AsRfSwitch(true);

    Serial.printf("  Fréquence : %.1f MHz | BW : %.0f kHz | SF%d | CR4/%d\n",
        LORA_FREQ, LORA_BW, LORA_SF, LORA_CR);
    Serial.println();
    Serial.println("  En attente des données GPS du T-Beam...");
    Serial.println();
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
    String received;
    int state = radio.receive(received);

    if (state == RADIOLIB_ERR_NONE) {
        rxCount++;
        float rssi = radio.getRSSI();
        float snr  = radio.getSNR();

        if (received.startsWith("GPS,")) {
            printGPS(received, rssi, snr);
        } else {
            printNoGPS(received, rssi, snr);
        }

        Serial.printf("  Total reçus : %d  |  Avec fix GPS : %d\n\n",
            rxCount, gpsCount);

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("  [!] Paquet corrompu (CRC error)");

    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.printf("  [!] Erreur RX : %d\n", state);
    }
}
