# Sensor Sensei — Résumé Backend API

## Architecture générale

```
LilyGo T-Beam        Heltec Gateway        Backend API           Stockage
(ESP32 + capteurs) ──LoRa──▶ (WiFi) ──HTTPS──▶ FastAPI  ──▶  PostgreSQL
                                                  │
                                                  └──▶  sensor.community
                                                         (background, optionnel)
```

Le backend joue le rôle de **point d'entrée unique** entre le monde physique (capteurs LoRa) et le reste de la stack (dashboards, plateforme open data).

---

## Choix techniques

| Composant | Choix | Raison |
|-----------|-------|--------|
| Framework | **FastAPI** | Validation automatique Pydantic, async natif, Swagger UI intégré |
| Base de données | **PostgreSQL 16** | Données temporelles, robuste, scalable |
| ORM | **SQLAlchemy 2 async** | Compatible asyncio, migrations Alembic |
| Containerisation | **Docker Compose** | Dev reproductible, même stack en prod |
| Approche API | **Contract-first** | `openapi.yaml` est la source de vérité |

---

## Approche Contract-First

L'API a été conçue **en partant du contrat** (`openapi.yaml`) plutôt que du code.

**Workflow :**
1. On modifie `openapi.yaml` en premier
2. On regénère les schémas Pydantic depuis le YAML (`datamodel-code-generator`)
3. On adapte les routes FastAPI pour respecter le contrat
4. Swagger UI sert **le fichier YAML** directement — pas ce que FastAPI génère depuis le code

**Avantage concret** : n'importe qui peut lire `openapi.yaml` et savoir exactement ce que fait l'API, sans lire le code Python. Facilite aussi l'intégration côté firmware (gateway Heltec).

---

## Endpoints

| Méthode | Path | Description |
|---------|------|-------------|
| `GET` | `/api/v1/health` | Healthcheck API + base de données |
| `POST` | `/api/v1/measurements` | **Ingestion** d'une mesure depuis la gateway |
| `GET` | `/api/v1/measurements` | Liste paginée, filtrable par device et plage de temps |
| `GET` | `/api/v1/devices` | Liste des capteurs connus |
| `GET` | `/api/v1/devices/{id}` | Détails d'un capteur |
| `PATCH` | `/api/v1/devices/{id}` | Mise à jour (ex. associer un ID sensor.community) |
| `GET` | `/api/v1/devices/{id}/latest` | Dernière mesure complète + état batterie courant |
| `GET` | `/api/v1/devices/{id}/battery/history` | Historique batterie uniquement |

---

## Modèle de données ingéré

Payload envoyé par la gateway Heltec (WiFi direct, architecture actuelle) :

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

Champs optionnels dans `sensors` :

| Champ | Statut | Raison |
|-------|--------|--------|
| `sensors.bme280` | **Obligatoire** | Toujours présent |
| `sensors.microphone` | Optionnel | Présent si micro I2S câblé |
| `sensors.dust` | Optionnel | Absent tant que le GP2Y n'est pas branché |

Toutes les valeurs sont validées (plages, types, champs obligatoires) avant insertion en base.

---

## Fonctionnalités clés

### Auto-enregistrement des devices
Un capteur inconnu est enregistré automatiquement à la première mesure reçue. Pas besoin de pré-configurer les devices dans la base.

### Idempotence
Chaque mesure porte un `message_id` unique. Si la gateway renvoie deux fois la même mesure (retry réseau), la deuxième est rejetée proprement (HTTP 409) sans doublon en base.

### Authentification par token
Chaque gateway s'authentifie avec un bearer token (`Authorization: Bearer <token>`). Plusieurs tokens supportés simultanément (un par gateway physique).

### Forward vers sensor.community
Après chaque ingestion réussie, les valeurs sont transmises en **background** (sans bloquer la réponse) à sensor.community via leur API :
- **Pin 1** → PM10 / PM2.5 (capteur GP2Y)
- **Pin 11** → Température, pression, humidité (BME280)

Le forward est désactivé par défaut (`SENSOR_COMMUNITY_ENABLED=false`) et s'active une fois les capteurs enregistrés sur la plateforme.

### Endpoint batterie dédié
`/battery/history` renvoie uniquement `timestamp + voltage + percentage + charging` — payload 10× plus léger que les mesures complètes, pensé pour les courbes de décharge et la prédiction d'autonomie.

---

## Structure du projet

```
backend/
├── openapi.yaml              ← contrat API (source de vérité)
├── app/
│   ├── main.py               ← point d'entrée FastAPI
│   ├── config.py             ← variables d'environnement (Pydantic Settings)
│   ├── db.py                 ← session SQLAlchemy async
│   ├── core/
│   │   ├── auth.py           ← vérification bearer token
│   │   ├── errors.py         ← handlers d'erreurs uniformes
│   │   └── openapi.py        ← sert openapi.yaml sur /openapi.json
│   ├── models/               ← tables SQLAlchemy (Device, Measurement)
│   ├── schemas/              ← schémas Pydantic générés depuis openapi.yaml
│   ├── routers/              ← endpoints (measurements, devices, battery, health)
│   └── workers/
│       └── sensor_community.py  ← forward async vers sensor.community
├── alembic/                  ← migrations base de données
├── scripts/
│   ├── simulate_gateway.py   ← simulateur de gateway pour le dev
│   └── generate_schemas.sh   ← régénération schemas depuis openapi.yaml
└── docker-compose.yml        ← api + postgres + mock + simulateur
```

---

## Environnement de développement

L'environnement de dev inclut :
- Un **mock HTTP** (`mendhak/http-https-echo`) qui reçoit les pushes sensor.community et les affiche — permet de tester le forward sans compte réel
- Un **simulateur de gateway** qui génère des capteurs fictifs avec random walk réaliste (température, PM, batterie avec cycles de charge/décharge)
- **Hot-reload** uvicorn activable via `RELOAD=true`
