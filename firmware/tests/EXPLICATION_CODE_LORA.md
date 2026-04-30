# Explication du code — Test de communication LoRa
## Projet T-IOT-902 | Communication T-Beam ↔ Heltec

---

## 1. Vue d'ensemble du système

```
┌─────────────────────────────┐          ┌──────────────────────────────┐
│   LILYGO T-Beam V1.2        │          │   Heltec WiFi LoRa 32 V3     │
│                             │          │                              │
│   Rôle : ÉMETTEUR           │          │   Rôle : RÉCEPTEUR           │
│   Puce LoRa : SX1276        │  ~868MHz │   Puce LoRa : SX1262         │
│   Microcontrôleur : ESP32   │ ~~~~~~~~>│   Microcontrôleur : ESP32-S3 │
│                             │  radio   │                              │
│   Envoie 1 paquet / 3 s     │          │   Reçoit et affiche RSSI/SNR │
└─────────────────────────────┘          └──────────────────────────────┘
```

Les deux cartes communiquent **sans fil** via la technologie **LoRa** à **868 MHz**.
Aucun fil ne les relie — juste les ondes radio.

---

## 2. Qu'est-ce que LoRa ?

**LoRa** (Long Range) est une technologie radio conçue pour les objets connectés (IoT).

| Caractéristique | Valeur dans ce test | Signification |
|---              |---                  |              ---|
| Fréquence       | 868 MHz            | Bande ISM libre en Europe |
| Bandwidth (BW)  | 125 kHz             | Largeur du canal radio |
| Spreading Factor| 7                   | Vitesse vs portée (SF7 = rapide, SF12 = longue portée) |
| Coding Rate (CR) | 4/5                | Niveau de correction d'erreur |
| Puissance         | 10 dBm            | ~10 mW — puissance d'émission |

**Avantage de LoRa** : peut couvrir plusieurs kilomètres avec très peu d'énergie.

---

## 3. Code de l'émetteur — T-Beam (fichier `lora_tbeam_sender/main.cpp`)

### 3.1 Les bibliothèques importées

```cpp
#include <Arduino.h>    // Fonctions de base Arduino (setup, loop, delay...)
#include <SPI.h>        // Protocole de communication avec la puce LoRa
#include <Wire.h>       // Protocole I2C pour parler au gestionnaire d'énergie
#include <RadioLib.h>   // Bibliothèque universelle pour les modules radio
#include <XPowersLib.h> // Contrôle du chip de gestion d'énergie AXP192
```

### 3.2 Déclaration des broches (pins)

```cpp
// Broches SPI — connexion physique entre l'ESP32 et la puce SX1276
#define LORA_SCK   5   // Horloge SPI
#define LORA_MISO  19  // Données sortant de la puce LoRa vers l'ESP32
#define LORA_MOSI  27  // Données entrant dans la puce LoRa depuis l'ESP32
#define LORA_NSS   18  // Sélection de la puce (Chip Select)
#define LORA_RST   23  // Reset de la puce LoRa
#define LORA_DIO0  26  // Interruption — signale la fin d'envoi/réception
#define LORA_DIO1  33  // Interruption secondaire

// Broches I2C — pour le gestionnaire d'énergie AXP192
#define PMU_SDA 21     // Données I2C
#define PMU_SCL 22     // Horloge I2C
```

> **Pourquoi autant de broches ?** La puce LoRa SX1276 est un composant séparé sur
> la carte. L'ESP32 lui parle via le protocole SPI (4 fils de données + 1 de contrôle).

### 3.3 Création de l'objet radio

```cpp
SX1276 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
```

Crée un objet `radio` de type SX1276. On lui donne les 4 broches de contrôle.
RadioLib s'occupe de tout le protocole bas niveau en interne.

### 3.4 Étape spéciale T-Beam : allumer la puce LoRa (fonction `initPMU`)

```cpp
XPowersAXP192 PMU;   // Objet représentant le chip de gestion d'énergie

bool initPMU() {
    Wire.begin(PMU_SDA, PMU_SCL);                          // Démarre le bus I2C
    PMU.begin(Wire, AXP192_SLAVE_ADDRESS, PMU_SDA, PMU_SCL); // Détecte l'AXP192
    PMU.setLDO2Voltage(3300);  // Configure la tension de sortie LDO2 à 3.3V
    PMU.enableLDO2();          // Active LDO2 — cela alimente physiquement le SX1276
    delay(100);                // Attend que l'alimentation soit stable
}
```

> **Pourquoi cette étape ?** Sur le T-Beam V1.2, la puce LoRa SX1276 ne reçoit pas
> directement le 3.3V de la carte. Son alimentation passe par un régulateur contrôlé
> (LDO2) du chip AXP192. Si on n'active pas LDO2, la puce LoRa est éteinte et
> `radio.begin()` échoue.

### 3.5 La fonction `setup()` — initialisation

```cpp
void setup() {
    Serial.begin(115200);      // Démarre la communication série (pour voir les logs)
    delay(2000);               // Attend 2s que le port série soit prêt

    initPMU();                 // ÉTAPE 1 : alimenter la puce LoRa

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI); // ÉTAPE 2 : démarrer le bus SPI

    radio.begin(868.0);        // ÉTAPE 3 : initialiser la puce à 868 MHz

    // ÉTAPE 4 : configurer les paramètres radio
    radio.setOutputPower(10);       // Puissance d'émission : 10 dBm
    radio.setBandwidth(125.0);      // Largeur de bande : 125 kHz
    radio.setSpreadingFactor(7);    // Facteur d'étalement : SF7
    radio.setCodingRate(5);         // Taux de codage : 4/5
}
```

### 3.6 La fonction `loop()` — envoi répété

```cpp
void loop() {
    counter++;                              // Incrémente le compteur de paquets
    String msg = "T-Beam#" + String(counter); // Crée le message : "T-Beam#1", "T-Beam#2"...

    int state = radio.transmit(msg);        // ENVOI du message par radio

    if (state == RADIOLIB_ERR_NONE) {
        // Succès : affiche le numéro, le message, et le temps d'occupation de l'air
        Serial.printf("OK — airtime %.0f ms", radio.getTimeOnAir(msg.length()) / 1000.0);
    } else {
        Serial.printf("ERREUR %d", state);  // Échec : affiche le code d'erreur
    }

    delay(3000);  // Attend 3 secondes avant le prochain envoi
}
```

> **Qu'est-ce que l'airtime ?** Le temps pendant lequel la fréquence radio est occupée
> pour transmettre un paquet. Avec SF7 + BW 125 kHz, un court message prend ~40 ms.

---

## 4. Code du récepteur — Heltec (fichier `lora_heltec_receiver/main.cpp`)

### 4.1 Différences avec l'émetteur

Le Heltec utilise la puce **SX1262** (plus récente que le SX1276 du T-Beam).
Il n'a pas besoin de gestionnaire d'énergie externe (pas d'AXP192).

```cpp
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
//                                              ^^^^ BUSY : spécifique SX1262
//                                                         indique quand la puce est occupée
```

### 4.2 Particularité du Heltec V3 : le commutateur RF

```cpp
radio.setDio2AsRfSwitch(true);
```

Sur le Heltec V3, la broche DIO2 de la puce SX1262 est câblée à un commutateur
qui bascule l'antenne entre le mode émission et réception. Sans cette ligne,
la réception ne fonctionnerait pas.

### 4.3 La fonction `loop()` — attente et réception

```cpp
void loop() {
    String received;               // Variable qui contiendra le message reçu
    int state = radio.receive(received); // ÉCOUTE — bloquant jusqu'à réception ou timeout

    if (state == RADIOLIB_ERR_NONE) {
        // Paquet reçu sans erreur
        rxCount++;
        Serial.printf("%s | RSSI: %.1f dBm | SNR: %.1f dB",
            received.c_str(),
            radio.getRSSI(),   // Puissance du signal reçu (plus proche de 0 = meilleur)
            radio.getSNR());   // Rapport signal/bruit (plus élevé = meilleur)

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        // Paquet reçu mais données corrompues (interférence radio)
        Serial.println("Paquet reçu avec erreur CRC");

    } else if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        // Erreur autre qu'un simple timeout (normal en l'absence de signal)
        Serial.printf("Erreur RX : %d", state);
    }
}
```

---

## 5. Comprendre les résultats obtenus

### Résultats du test réel :

```
#      | Message    | RSSI (dBm) | SNR (dB)
     1 | T-Beam#1   |    -27.0   |   12.5
     2 | T-Beam#2   |    -28.0   |   12.0
    20 | T-Beam#20  |    -24.0   |   12.0
```

### Interprétation du RSSI

Le **RSSI** (Received Signal Strength Indicator) mesure la puissance du signal reçu.

| Valeur RSSI | Qualité |
|---|---|
| 0 à -50 dBm | Excellent — cartes très proches |
| -50 à -80 dBm | Bon — quelques mètres à dizaines de mètres |
| -80 à -100 dBm | Correct — centaines de mètres |
| < -110 dBm | Faible — limite de portée |

→ Nos valeurs (-21 à -38 dBm) = **signal excellent**, les deux cartes sont côte à côte.

### Interprétation du SNR

Le **SNR** (Signal to Noise Ratio) mesure la qualité du signal par rapport au bruit.

| Valeur SNR | Qualité |
|---|---|
| > 10 dB | Excellent |
| 5 à 10 dB | Bon |
| 0 à 5 dB | Limite |
| < 0 dB | Très bruité (mais LoRa peut encore décoder) |

→ Nos valeurs (~12 dB) = **liaison parfaitement propre**, aucune interférence.

### 0 paquet perdu sur 20 envois = liaison fiable ✓

---

## 6. Paramètres LoRa partagés (obligatoires)

Pour que deux modules LoRa se comprennent, **tous les paramètres doivent être
identiques** des deux côtés. C'est pourquoi les deux fichiers contiennent les mêmes valeurs :

```
Émetteur (T-Beam)    ==    Récepteur (Heltec)
LORA_FREQ = 868.0    ==    LORA_FREQ = 868.0   ← même fréquence
LORA_BW   = 125.0    ==    LORA_BW   = 125.0   ← même largeur de bande
LORA_SF   = 7        ==    LORA_SF   = 7        ← même facteur d'étalement
LORA_CR   = 5        ==    LORA_CR   = 5        ← même taux de codage
```

Si un seul paramètre diffère → les paquets ne sont pas reçus.

---

## 7. Résumé du flux d'exécution

```
T-Beam (émetteur)                    Heltec (récepteur)
─────────────────                    ──────────────────
setup():                             setup():
  1. Allumer SX1276 via AXP192         1. Démarrer SPI
  2. Démarrer SPI                      2. Initialiser SX1262
  3. Initialiser SX1276                3. Activer commutateur RF (DIO2)
  4. Configurer paramètres LoRa        4. Configurer mêmes paramètres LoRa

loop() — toutes les 3 secondes:      loop() — en attente permanente:
  1. Créer message "T-Beam#N"          1. Écouter le canal radio
  2. radio.transmit(message)  ──>      2. radio.receive(message reçu)
  3. Afficher "OK"                     3. Afficher message + RSSI + SNR
  4. Attendre 3 secondes               4. Recommencer l'écoute
```
