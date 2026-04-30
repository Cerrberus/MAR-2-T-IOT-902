/**
 * TEST — Émetteur LoRa + GPS sur LILYGO T-Beam V1.2
 * Lit les coordonnées GPS et les envoie par LoRa toutes les 5 secondes.
 *
 * Puce LoRa : SX1276 (868 MHz)
 * GPS       : u-blox NEO (UART1 — GPIO 34 RX, GPIO 12 TX)
 * Récepteur : Heltec WiFi LoRa 32 V3 (SX1262)
 *
 * Flasher  : pio run -e lora_tbeam_sender_test --target upload
 * Monitor  : pio device monitor -e lora_tbeam_sender_test
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RadioLib.h>
#include <XPowersLib.h>
#include <TinyGPSPlus.h>

// ─── Pins SPI LoRa (T-Beam V1.2) ─────────────────────────────────────────────
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_NSS   18
#define LORA_RST   23
#define LORA_DIO0  26
#define LORA_DIO1  33

// ─── Pins GPS UART (T-Beam V1.2) ─────────────────────────────────────────────
#define GPS_RX_PIN  34   // ESP32 reçoit depuis GPS TX
#define GPS_TX_PIN  12   // ESP32 envoie vers GPS RX
#define GPS_BAUD    9600

// ─── I2C AXP192 ──────────────────────────────────────────────────────────────
#define PMU_SDA 21
#define PMU_SCL 22

// ─── Paramètres LoRa ──────────────────────────────────────────────────────────
#define LORA_FREQ   868.0
#define LORA_BW     125.0
#define LORA_SF     7
#define LORA_CR     5
#define LORA_POWER  10

XPowersAXP192  PMU;
SX1276         radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
TinyGPSPlus    gps;
HardwareSerial gpsSerial(1);   // UART1 de l'ESP32

int counter = 0;

// ─── Initialisation AXP192 ───────────────────────────────────────────────────
bool initPMU() {
    Wire.begin(PMU_SDA, PMU_SCL);
    if (!PMU.begin(Wire, AXP192_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        return false;
    }
    PMU.setLDO2Voltage(3300);
    PMU.enableLDO2();   // Alimente le SX1276 (LoRa)
    PMU.setLDO3Voltage(3300);
    PMU.enableLDO3();   // Alimente le module GPS
    delay(200);
    return true;
}

// ─── Lit le GPS pendant `ms` millisecondes ───────────────────────────────────
void feedGPS(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        while (gpsSerial.available()) {
            gps.encode(gpsSerial.read());
        }
    }
}

// ─── Formate le paquet GPS à envoyer ─────────────────────────────────────────
String buildPacket() {
    counter++;
    if (gps.location.isValid() && gps.location.age() < 2000) {
        return String("GPS,") +
               String(gps.location.lat(), 6) + "," +
               String(gps.location.lng(), 6) + "," +
               String(gps.altitude.meters(), 1) + "," +
               String(gps.satellites.value()) + "," +
               String(counter);
    } else {
        return String("NOGPS,") +
               String(gps.satellites.value()) + "sat," +
               String(counter);
    }
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║  TEST LoRa+GPS — T-Beam V1.2 (émetteur)  ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();

    Serial.print("  AXP192 (LoRa + GPS power)... ");
    Serial.println(initPMU() ? "OK" : "[!] Echec");

    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("  GPS UART démarré (GPIO34 RX, GPIO12 TX)");

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI);

    Serial.print("  SX1276 init... ");
    int state = radio.begin(LORA_FREQ);
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("OK");
    } else {
        Serial.printf("[!] Echec (code %d)\n", state);
        while (true) delay(1000);
    }

    radio.setOutputPower(LORA_POWER);
    radio.setBandwidth(LORA_BW);
    radio.setSpreadingFactor(LORA_SF);
    radio.setCodingRate(LORA_CR);

    Serial.println();
    Serial.println("  En attente du fix GPS (peut prendre quelques minutes en extérieur)...");
    Serial.println("  ─────────────────────────────────────────────────────────────────────");
    Serial.println("  #    | Paquet envoyé                          | Statut");
    Serial.println("  ─────────────────────────────────────────────────────────────────────");
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
    // Lit le GPS pendant 4 secondes entre chaque envoi
    feedGPS(4000);

    String packet = buildPacket();
    int state = radio.transmit(packet);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.printf("  %4d | %-38s | OK\n", counter, packet.c_str());
    } else {
        Serial.printf("  %4d | %-38s | ERR %d\n", counter, packet.c_str(), state);
    }

    delay(1000);
}
