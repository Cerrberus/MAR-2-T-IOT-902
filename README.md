# Sensor Sensei — Firmware MAR-2

> Epitech T-IOT-902 — Nœud capteur environnemental sur ESP32 TTGO T-Beam.  
> Compatible avec le projet [sensor.community](https://sensor.community), étendu avec une communication longue portée via LoRa.

---

## Contexte

Le projet [sensor.community](https://sensor.community) cartographie la qualité de l'air à l'échelle mondiale grâce à des capteurs construits par des citoyens. Sa conception originale repose sur le WiFi, ce qui limite son déploiement en zone rurale et implique une consommation énergétique élevée.

Ce firmware propose une architecture alternative :

```
[Nœud capteur — batterie]
         |
    LoRa (portée km, très faible consommation)
         |
[Passerelle LoRa ↔ WiFi]
         |
        HTTP
         |
[Serveur sensor.community]
```

Le nœud capteur est autonome (alimenté par batterie). Il mesure les données environnementales et les transmet via LoRa à une passerelle dédiée. La passerelle relaie ensuite les données vers l'API sensor.community via WiFi. Chaque capteur n'a plus besoin de sa propre connexion WiFi, ce qui réduit drastiquement la consommation et étend la portée opérationnelle.

---

## Matériel

| Composant | Rôle |
|---|---|
| **TTGO T-Beam** (ESP32) | Microcontrôleur principal + GPS + LoRa intégrés |
| **Sharp GP2Y1010AU0F** | Capteur optique de poussière — estimation PM2.5 |
| **Bosch BMP280** | Température, pression atmosphérique, altitude |
| **u-blox NEO-6M / M8N** | GPS — position, vitesse, altitude |

---

## Câblage

### Capteur de poussière (Sharp GP2Y1010AU0F)

| Signal | GPIO ESP32 | Remarques |
|---|---|---|
| AOUT (sortie analogique) | 36 (VP / ADC1) | ADC 12 bits, atténuation 0 dB (0–1,1 V) |
| ILED (commande LED IR) | 13 | Actif HIGH — impulsion de 280 µs par échantillon |

La séquence de mesure respecte le timing de la datasheet Sharp GP2Y1010AU0F :

```
LED ON ──── 280 µs ──── échantillon ──── 40 µs ──── LED OFF ──── 9680 µs ──── (répéter)
```

### BMP280 (I2C)

| Signal | GPIO ESP32 |
|---|---|
| SDA | 21 |
| SCL | 22 |
| Adresse I2C | `0x76` (SDO → GND) ou `0x77` (SDO → VCC) |

### GPS (UART1)

| Signal | GPIO ESP32 |
|---|---|
| RX (ESP32 reçoit les trames NMEA) | 34 |
| TX (ESP32 envoie des commandes) | 12 |
| Débit | 9600 baud |

---

## Compilation et flash

Ce projet utilise [PlatformIO](https://platformio.org/).

```bash
# Installer PlatformIO CLI
pip install platformio

# Compiler
cd firmware
pio run

# Flasher
pio run --target upload

# Ouvrir le moniteur série (115200 baud)
pio device monitor
```

La carte cible est `ttgo-t-beam`. Les dépendances sont résolues automatiquement via `platformio.ini` :

```ini
lib_deps =
    adafruit/Adafruit BMP280 Library @ ^2.6.8
    adafruit/Adafruit Unified Sensor @ ^1.1.14
    mikalhart/TinyGPSPlus @ ^1.0.3
```

---

## Sortie moniteur série

Chaque cycle de mesure affiche un rapport formaté à 115200 baud :

```
========================================
  t = 12543 ms
========================================
  [GPS]
  Satellites : 7   HDOP: 0.9
  Position   : 48.858844, 2.294351
  Altitude   : 35.0 m
  Vitesse    : 0.0 km/h

  [BMP280]
  Temp     : 22.34 degC
  Pression : 1013.25 hPa
  Altitude : 34.8 m

  [Poussiere]  etat: pret
  OFF raw  :   412.00 ADC
  ON  raw  :   418.75 ADC
  Delta    :    +6.75 ADC  (base: 6.10)
  Relatif  :    +0.65 ADC

  Niveau   :   8%  faible  [##------------------]
  PM2.5 ~  : 40.0 ug/m3
```

---

## Format JSON

La fonction `projectDustAppendJson()` sérialise les données du capteur dans un fragment JSON destiné à être intégré dans la charge utile envoyée à sensor.community :

```json
{
  "dust_ready": true,
  "dust_state": "pret",
  "dust_off_raw": 412.00,
  "dust_on_raw": 418.75,
  "dust_delta": 6.75,
  "dust_base": 6.10,
  "dust_relative": 0.65,
  "dust_level_pct": 8,
  "dust_label": "faible",
  "dust_pm25_estimated_ug_m3": 40.0
}
```

### États du capteur de poussière

| État | Signification |
|---|---|
| `aucun_signal` | Aucun signal IR détecté — vérifier le câblage |
| `stabilisation` | En attente de lectures valides consécutives |
| `calibration` | Construction de la baseline ambiante (8 premières mesures) |
| `pret` | Calibré et opérationnel — données exploitables |

### Niveaux de qualité PM2.5

| Libellé | Indice | PM2.5 estimé |
|---|---|---|
| `silence` | 0–9 % | 0–45 µg/m³ |
| `faible` | 10–34 % | 45–170 µg/m³ |
| `moyen` | 35–64 % | 170–320 µg/m³ |
| `fort` | 65–89 % | 320–445 µg/m³ |
| `tres fort` | 90–100 % | 445–500 µg/m³ |

> **Note :** les valeurs PM2.5 sont estimées à partir d'un delta ADC relatif, calibré sur la baseline ambiante locale. Il s'agit de valeurs indicatives, non de mesures de laboratoire.

---

## Algorithme d'acquisition — capteur de poussière

Le module poussière applique plusieurs couches de réduction du bruit :

1. **Moyenne courte** — 4 lectures ADC consécutives sont moyennées par échantillon pour réduire le bruit électronique.
2. **Paires OFF/ON** — 48 paires LED éteinte / LED allumée par cycle ; la soustraction OFF/ON annule la dérive de lumière ambiante.
3. **Fenêtre glissante** — les 10 dernières valeurs brutes OFF/ON sont conservées dans un buffer circulaire ; leur moyenne constitue la valeur de travail.
4. **Calibration de baseline** — les 8 premières mesures valides construisent une baseline ambiante par moyenne incrémentale. Delta relatif = delta mesuré − baseline (plancher à 0).
5. **Lissage d'affichage** — la montée est bridée (+1,0 ADC/cycle max) ; la descente suit une moyenne exponentielle (α = 0,75).

---

## Principes de conception

Ce firmware respecte les principes **S.O.L.I.D.** exigés par T-IOT-902 :

- **Responsabilité unique** — chaque capteur possède ses propres fonctions `setup`, `acquireReading` et `appendJson` isolées. Aucun état partagé entre modules.
- **Ouvert/fermé** — ajouter un nouveau capteur ne nécessite qu'un nouveau bloc auto-contenu ; `setup()` et `loop()` s'étendent par composition, sans modification des modules existants.
- **Substitution de Liskov** — chaque struct `*Reading` expose un champ `ready` pour que les appelants gèrent uniformément l'absence de matériel.
- **Ségrégation des interfaces** — les modules n'exposent que les trois fonctions nécessaires (`setup`, `acquire`, `appendJson`) ; l'état interne et les helpers sont `static`.
- **Inversion des dépendances** — les numéros de broches et les constantes de timing sont déclarés en `static const` en tête de chaque module, jamais en dur dans la logique.

### Énergie

Un projet de surveillance environnementale doit maîtriser sa propre empreinte :

- Le radio LoRa remplace le WiFi sur le nœud capteur — consommation en émission inférieure de plusieurs ordres de grandeur.
- Le deep sleep entre les cycles de mesure sera ajouté pour maximiser l'autonomie sur batterie.
- L'atténuation ADC est réglée à `ADC_0db` (plage 0–1,1 V) pour maximiser la résolution sans étage d'amplification inutile.

---

## Checklist de livraison (T-IOT-902)

- [x] Listing des fonctionnalités du firmware existant (ce document)
- [x] Nœud capteur autonome — mesure et transmet via LoRa
- [ ] Passerelle — relaie les paquets LoRa vers sensor.community via WiFi
- [x] Nouveau firmware conservant la compatibilité capteurs (BMP280, GPS, poussière)
- [ ] Passe d'optimisation énergétique / deep sleep
- [x] Documentation firmware
- [ ] Visualisation des données

---

## Structure du projet

```
MAR-2-T-IOT-902/
└── firmware/
    ├── platformio.ini       # Configuration PlatformIO — carte : ttgo-t-beam
    └── src/
        └── main.cpp         # Tous les modules capteurs + setup/loop
```

---

*Epitech — T-IOT-902 — Sensor Sensei*
