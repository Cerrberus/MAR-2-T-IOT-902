# Sensor Sensei — Spec d'intégration firmware → API

Document à destination de l'équipe firmware.  
Toute modification du contrat doit d'abord passer par l'équipe API (`openapi.yaml`).

---

## Endpoint d'ingestion

```
POST /api/v1/measurements
Host: <adresse IP du serveur>:8000
Authorization: Bearer <token>
Content-Type: application/json
```

Le token sera fourni par l'équipe API. Ne pas le commiter dans le code source.

Réponses attendues :

| Code | Signification |
|------|---------------|
| `201` | Mesure acceptée et stockée |
| `401` | Token manquant ou invalide |
| `409` | `message_id` déjà reçu (doublon ignoré, pas d'erreur fatale) |
| `422` | Payload mal formé (voir `details` dans la réponse) |

---

## Payload JSON

```json
{
  "message_id": "heltec-001-1746000000",
  "device": {
    "id": "heltec-001",
    "firmware_version": "1.0.0",
    "location": {
      "latitude": 43.2965,
      "longitude": 5.3698
    }
  },
  "timestamp": "2026-04-30T10:00:00Z",
  "transmission": {
    "protocol": "WiFi",
    "rssi": -65,
    "snr": 0.0
  },
  "battery": {
    "voltage_v": 3.85,
    "percentage": 78,
    "charging": false
  },
  "sensors": {
    "bme280": {
      "type": "BMP280",
      "temperature": 22.4,
      "pressure": 1013.25,
      "humidity": 55.0
    },
    "microphone": {
      "type": "I2S",
      "level": 4200
    }
  }
}
```

---

## Champs obligatoires / optionnels

### Racine

| Champ | Obligatoire | Description |
|-------|-------------|-------------|
| `message_id` | Oui | Identifiant unique par mesure. Format recommandé : `{device_id}-{timestamp_unix}`. Sert à dédupliquer les retransmissions. |
| `device.id` | Oui | Identifiant stable du device (ex. `heltec-001`). Le device est créé automatiquement à la première mesure. |
| `device.firmware_version` | Oui | Version semver du firmware (ex. `1.0.0`) |
| `device.location` | Oui | Coordonnées GPS fixes du device |
| `timestamp` | Oui | Horodatage ISO 8601 UTC de la mesure (`Z` obligatoire). Utiliser NTP. |
| `transmission.protocol` | Oui | `"WiFi"`, `"LoRa"` ou `"BLE"` |
| `transmission.rssi` | Oui | Signal WiFi en dBm (`WiFi.RSSI()` sur ESP32) |
| `transmission.snr` | Oui | Mettre `0.0` si non applicable (mode WiFi) |
| `battery.voltage_v` | Oui | Tension batterie lue sur ADC (0–5 V) |
| `battery.percentage` | Oui | Niveau batterie 0–100 |
| `battery.charging` | Oui | `true` si en charge |
| `sensors.bme280` | Oui | Température, pression, humidité |
| `sensors.bme280.humidity` | Non | `null` si le capteur est un BMP280 sans humidité |

### Capteurs optionnels

| Champ | Obligatoire | Description |
|-------|-------------|-------------|
| `sensors.microphone` | Non | Présent uniquement si micro I2S câblé |
| `sensors.microphone.type` | Oui si présent | Modèle du micro (ex. `"I2S"`) |
| `sensors.microphone.level` | Oui si présent | Amplitude moyenne sur le buffer I2S (entier positif) |
| `sensors.dust` | Non | Absent tant que le capteur GP2Y n'est pas câblé |
| `sensors.dust.type` | Oui si présent | `"GP2Y1010AU0F"` |
| `sensors.dust.P1` | Oui si présent | PM10 en µg/m³ (≥ 0) |
| `sensors.dust.P2` | Oui si présent | PM2.5 en µg/m³ (≥ 0) |

---

## Génération du `message_id`

Le `message_id` doit être **unique par device**. Si la gateway retransmet une mesure (retry après timeout), l'API détecte le doublon via ce champ et renvoie `409` sans stocker deux fois.

Format recommandé en C++ :

```cpp
char msgId[64];
snprintf(msgId, sizeof(msgId), "%s-%lu", DEVICE_ID, (unsigned long)(unixTimestamp));
```

Avec NTP : utiliser `time(nullptr)` après sync.  
Sans NTP : utiliser `millis()` en fallback (risque de collision après reboot — utiliser NTP dès que possible).

---

## Exemple complet Arduino / ESP32

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

const char* API_URL   = "http://192.168.1.42:8000/api/v1/measurements";
const char* API_TOKEN = "dev-token";   // ← remplacer par le token fourni
const char* DEVICE_ID = "heltec-001";

void sendMeasurement(float temp, float humidity, float pressure, int micLevel) {
    if (WiFi.status() != WL_CONNECTED) return;

    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

    char msgId[64];
    snprintf(msgId, sizeof(msgId), "%s-%lu", DEVICE_ID, (unsigned long)now);

    StaticJsonDocument<512> doc;
    doc["message_id"]                         = msgId;
    doc["device"]["id"]                       = DEVICE_ID;
    doc["device"]["firmware_version"]         = "1.0.0";
    doc["device"]["location"]["latitude"]     = 43.2965;
    doc["device"]["location"]["longitude"]    = 5.3698;
    doc["timestamp"]                          = timestamp;
    doc["transmission"]["protocol"]           = "WiFi";
    doc["transmission"]["rssi"]               = WiFi.RSSI();
    doc["transmission"]["snr"]                = 0.0;
    doc["battery"]["voltage_v"]               = 3.7;   // TODO: lire ADC
    doc["battery"]["percentage"]              = 100;
    doc["battery"]["charging"]                = false;
    doc["sensors"]["bme280"]["type"]          = "BMP280";
    doc["sensors"]["bme280"]["temperature"]   = temp;
    doc["sensors"]["bme280"]["pressure"]      = pressure;
    doc["sensors"]["bme280"]["humidity"]      = humidity;
    doc["sensors"]["microphone"]["type"]      = "I2S";
    doc["sensors"]["microphone"]["level"]     = micLevel;
    // doc["sensors"]["dust"] → ne pas envoyer tant que le GP2Y n'est pas câblé

    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(API_URL);
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("Authorization", String("Bearer ") + API_TOKEN);
    int code = http.POST(body);
    Serial.printf("[API] %d\n", code);
    http.end();
}
```

---

## Ce qu'il faut fournir à l'équipe API quand le GP2Y est branché

1. Confirmer le modèle exact du capteur (ex. `GP2Y1010AU0F`)
2. Les unités de sortie (µg/m³ ou valeur brute ADC ?)
3. Le mapping vers P1 (PM10) et P2 (PM2.5)

L'équipe API mettra à jour `openapi.yaml` en conséquence — ne pas envoyer `sensors.dust` avant alignement.
