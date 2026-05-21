# Sensor Sensei - Backend API

FastAPI + PostgreSQL backend pour le projet de monitoring de qualité d'air.

```
LilyGo T-Beam  ──LoRa──▶  Heltec Gateway  ──WiFi/HTTPS──▶  FastAPI  ──▶  PostgreSQL
                                                             │
                                                             └──▶  sensor.community (async, optionnel)
```

**Contract-first** : [openapi.yaml](openapi.yaml) est la source de vérité. Modifier le contrat = modifier ce fichier en premier.

---

## 1. Démarrage rapide (dev)

```bash
cd backend
cp .env.example .env        # les valeurs par défaut suffisent pour le dev
docker compose up --build   # démarre api + postgres

# dans un 2e terminal : appliquer les migrations
docker compose run --rm migrate
```

Vérifier que tout tourne :

```bash
curl http://localhost:8000/api/v1/health
# {"status":"ok","database":"ok","version":"1.0.0"}
```

Swagger UI interactif : **http://localhost:8000/docs**

---

## 2. Tester sans capteur réel (simulateur)

Le simulateur `scripts/simulate_gateway.py` imite une gateway Heltec : il génère des payloads réalistes (random walk sur température, PM, batterie) et les POSTe sur l'API.

> **Note** : le simulateur tourne dans Docker, il ne peut pas joindre `localhost:8000`.
> Il faut toujours passer `--base-url http://api:8000` pour cibler le service API
> sur le réseau interne Docker.

### Envoyer un seul batch (test rapide)

```bash
docker compose run --rm simulator python scripts/simulate_gateway.py \
  --base-url http://api:8000 --token dev-token --once
```

### Simuler 2 capteurs en continu (toutes les 5 s)

```bash
docker compose --profile sim up simulator
```

### Remplir un historique de 200 points par capteur (pour tester les dashboards)

```bash
docker compose run --rm simulator python scripts/simulate_gateway.py \
  --base-url http://api:8000 --token dev-token --history 200 --devices 2
```

### Changer les paramètres via `.env` (pour le profil `sim` uniquement)

```bash
SIM_DEVICES=3       # nombre de capteurs simulés
SIM_INTERVAL=10     # secondes entre deux batches
SIM_TOKEN=changeme  # doit correspondre à GATEWAY_TOKENS dans .env
```

### Tester manuellement avec curl

```bash
# Injecter une mesure
curl -s -X POST http://localhost:8000/api/v1/measurements \
  -H "Authorization: Bearer changeme" \
  -H "Content-Type: application/json" \
  -d '{
    "message_id": "test-001-1",
    "device": {
      "id": "test-001",
      "firmware_version": "1.0.0",
      "location": {"latitude": 43.2965, "longitude": 5.3698}
    },
    "timestamp": "2026-04-30T12:00:00Z",
    "transmission": {"protocol": "LoRa", "rssi": -87, "snr": 7.5},
    "battery": {"voltage_v": 3.85, "percentage": 78, "charging": false},
    "sensors": {
      "dust": {"type": "GP2Y1010AU0F", "P1": 45.2, "P2": 12.8},
      "bme280": {"type": "BMP280", "temperature": 22.4, "pressure": 1013.25, "humidity": null}
    }
  }'

# Vérifier la dernière mesure
curl -s http://localhost:8000/api/v1/devices/test-001/latest \
  -H "Authorization: Bearer changeme" | python3 -m json.tool

# Historique batterie
curl -s "http://localhost:8000/api/v1/devices/test-001/battery/history?limit=10" \
  -H "Authorization: Bearer changeme" | python3 -m json.tool
```

> **Tip** : l'exemple pré-rempli dans Swagger UI (`/docs`) correspond exactement au payload du capteur - utilise le bouton "Try it out".

---

## 3. Une fois les capteurs enregistrés sur sensor.community

Quand tu auras [enregistré tes capteurs](https://devices.sensor.community/) et récupéré leurs **sensor IDs** numériques (ex. `12345`), faire en deux étapes :

### Étape 1 - Associer l'ID sensor.community à chaque device

```bash
# Remplacer <device-id> par l'id du capteur (ex. tbem-lora32-001)
# et <sensor-community-id> par le numéro donné lors de l'enregistrement

curl -s -X PATCH http://localhost:8000/api/v1/devices/<device-id> \
  -H "Authorization: Bearer changeme" \
  -H "Content-Type: application/json" \
  -d '{"sensor_community_id": <sensor-community-id>}'
```

Vérifier :

```bash
curl -s http://localhost:8000/api/v1/devices/<device-id> \
  -H "Authorization: Bearer changeme" | python3 -m json.tool
# "sensor_community_id": <sensor-community-id>  ← doit apparaître
```

### Étape 2 - Activer le forward dans `.env`

```bash
# .env
SENSOR_COMMUNITY_ENABLED=true
SENSOR_COMMUNITY_URL=https://api.sensor.community/v1/push-sensor-data/
```

Puis redémarrer l'API :

```bash
docker compose restart api
```

À partir de là, chaque ingestion (`POST /api/v1/measurements`) envoie automatiquement les valeurs à sensor.community **en background** (sans bloquer la réponse à la gateway). Les logs confirment les pushs :

```
INFO sensor.community push ok sensor=12345 pin=1 status=200
INFO sensor.community push ok sensor=12345 pin=11 status=200
```

> **Pin 1** = poussière (P1/P2 du GP2Y)  
> **Pin 11** = BME280 (température, pression, humidité)

Les capteurs sans `sensor_community_id` sont simplement ignorés (log DEBUG) - utile si tu ajoutes un capteur en dev sans l'avoir encore enregistré.

---

## 4. Modifier le contrat (contract-first)

Workflow à suivre pour tout changement d'API :

```bash
# 1. Editer openapi.yaml
# 2. Regénérer les schémas Pydantic
./scripts/generate_schemas.sh

# 3. Adapter les routers si besoin
# 4. Créer une migration si le modèle DB change
docker compose exec api alembic revision --autogenerate -m "ma migration"
docker compose run --rm migrate
```

---

## 5. Référence des endpoints

| Méthode | Path | Auth | Description |
|---------|------|------|-------------|
| GET | `/api/v1/health` | - | Healthcheck (db inclus) |
| POST | `/api/v1/measurements` | token | Ingestion depuis gateway |
| GET | `/api/v1/measurements` | token | Liste paginée (filtres : device_id, from, to) |
| GET | `/api/v1/devices` | token | Liste des capteurs connus |
| GET | `/api/v1/devices/{id}` | token | Détails d'un capteur |
| PATCH | `/api/v1/devices/{id}` | token | Mettre à jour le sensor_community_id |
| GET | `/api/v1/devices/{id}/latest` | token | Dernière mesure complète + batterie courante |
| GET | `/api/v1/devices/{id}/battery/history` | token | Historique batterie uniquement |

Auth : `Authorization: Bearer <token>` (tokens configurés dans `GATEWAY_TOKENS`).

---

## 6. Commandes utiles

```bash
# Logs en direct
docker compose logs -f api

# Shell dans le conteneur api
docker compose exec api bash

# Lancer les tests
docker compose exec api pytest

# Vérifier que l'implémentation respecte le contrat openapi.yaml
docker compose exec api schemathesis run http://localhost:8000/openapi.json \
  -H "Authorization: Bearer changeme"

# Nouvelle migration
docker compose exec api alembic revision --autogenerate -m "description"

# Appliquer les migrations
docker compose run --rm migrate

# Réinitialiser la base (⚠ supprime toutes les données)
docker compose down -v && docker compose up -d db && docker compose run --rm migrate
```
