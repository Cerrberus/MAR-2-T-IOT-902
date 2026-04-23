# Sensor Sensei — Backend API

FastAPI + PostgreSQL backend for the Sensor Sensei IoT project.

## Architecture

```
LilyGo T-Beam  ──LoRa──▶  Heltec Gateway  ──WiFi/HTTPS──▶  FastAPI  ──▶  PostgreSQL
                                                             │
                                                             └──▶  sensor.community (async)
```

Contract-first: [openapi.yaml](openapi.yaml) is the source of truth. Pydantic schemas are generated from it.

## Quick start

```bash
cp .env.example .env
docker compose up --build
```

Then open:
- API: http://localhost:8000
- Swagger UI: http://localhost:8000/docs
- OpenAPI JSON: http://localhost:8000/openapi.json

## Development

```bash
# regenerate Pydantic schemas from openapi.yaml
./scripts/generate_schemas.sh

# run migrations
docker compose exec api alembic upgrade head

# create a new migration
docker compose exec api alembic revision --autogenerate -m "description"

# run tests
docker compose exec api pytest

# contract testing
docker compose exec api schemathesis run http://localhost:8000/openapi.json
```

## Authentication

Gateways authenticate with a bearer token configured via `GATEWAY_TOKENS` (comma-separated list).

```
Authorization: Bearer dev-token
```

## Endpoints

See [openapi.yaml](openapi.yaml) for the full contract.

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/v1/measurements` | Ingest a measurement (gateway) |
| GET | `/api/v1/measurements` | List measurements |
| GET | `/api/v1/devices` | List devices |
| GET | `/api/v1/devices/{id}` | Device details |
| GET | `/api/v1/devices/{id}/latest` | Latest measurement + current battery |
| GET | `/api/v1/devices/{id}/battery/history` | Battery time-series only |
| GET | `/api/v1/health` | Healthcheck |
