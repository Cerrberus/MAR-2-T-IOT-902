# Sensor Sensei — IOT-902

Projet de monitoring de qualité d'air via capteurs LoRa.

```
[T-Beam capteur] ──LoRa──▶ [Heltec gateway] ──HTTPS──▶ [FastAPI] ──▶ [PostgreSQL]
                                                              │
                                                              └──▶ [React dashboard]
```

---

## Prérequis

| Outil | Version minimale |
|-------|-----------------|
| Docker + Docker Compose | v2+ |
| Node.js | v18+ |
| PlatformIO CLI | (firmware uniquement) |

---

## 1. Backend (FastAPI + PostgreSQL)

```bash
cd backend
cp .env.example .env          # les valeurs par défaut suffisent en dev
docker compose up --build -d  # démarre api (port 8000) + postgres (port 5432)

# Dans un 2e terminal : appliquer les migrations + seed
docker compose run --rm migrate
```

Vérifier que tout tourne :

```bash
curl http://localhost:8000/api/v1/health
# {"status":"ok","database":"ok","version":"1.0.0"}
```

Swagger UI interactif : **http://localhost:8000/docs**

> Pour voir les logs en direct : `docker compose logs -f api`

---

## 2. Frontend (React + Vite)

```bash
cd frontend
cp .env.example .env   # optionnel en dev, le proxy Vite pointe déjà sur localhost:8000
npm install
npm run dev
```

L'interface est accessible sur **http://localhost:5173**

> Le frontend nécessite que le backend tourne sur `localhost:8000`.

---

## 3. Simuler des capteurs (sans matériel)

Pour tester le dashboard sans capteurs physiques, utilise le simulateur intégré :

```bash
cd backend

# Envoyer un seul batch (test rapide)
docker compose run --rm simulator python scripts/simulate_gateway.py \
  --base-url http://api:8000 --token changeme --once

# Simuler 2 capteurs en continu (toutes les 5 s)
docker compose --profile sim up simulator

# Remplir un historique de 200 points par capteur
docker compose run --rm simulator python scripts/simulate_gateway.py \
  --base-url http://api:8000 --token changeme --history 200 --devices 2
```

---

## 4. Firmware (ESP32 / PlatformIO)

```bash
cd firmware

# Flasher le capteur principal (Heltec WiFi LoRa 32 V3)
pio run -e heltec_wifi_lora_32_V3 --target upload

# Ouvrir le moniteur série
pio device monitor -e heltec_wifi_lora_32_V3
```

Les autres environnements disponibles dans `platformio.ini` :
- `lilygo_tbeam_test` — T-Beam LORA32
- `lora_tbeam_sender_test` — émetteur LoRa T-Beam
- `lora_heltec_receiver_test` — récepteur LoRa Heltec

---

## Structure du projet

```
MAR-2-T-IOT-902/
├── backend/          # FastAPI + PostgreSQL + Alembic
│   ├── app/          # code source de l'API
│   ├── scripts/      # seed, simulateur, migrations
│   ├── docker-compose.yml
│   └── openapi.yaml  # contrat API (source de vérité)
├── frontend/         # React + Vite + Recharts + Leaflet
│   └── src/
├── firmware/         # ESP32 / Arduino / PlatformIO
│   ├── src/          # firmware principal
│   └── tests/        # tests unitaires par composant
└── docs/             # spécifications et résumés d'API
```

---

## Variables d'environnement clés

| Variable | Défaut | Description |
|----------|--------|-------------|
| `GATEWAY_TOKENS` | `changeme` | Token Bearer attendu par l'API |
| `POSTGRES_USER/PASSWORD/DB` | `sensei` | Credentials PostgreSQL |
| `API_PORT` | `8000` | Port exposé par l'API |
| `RELOAD` | `false` | Hot-reload uvicorn (dev uniquement) |
| `SENSOR_COMMUNITY_ENABLED` | `false` | Forward vers sensor.community |

Voir `backend/.env.example` pour la liste complète.
