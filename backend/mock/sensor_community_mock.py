"""Minimal mock of the sensor.community push API for local demos.

Accepts POSTs on /v1/push-sensor-data/, logs what was received, and keeps
the last N requests in memory so you can inspect them from a browser.

Not part of the production API. Runs as its own service in docker-compose
under the `sim` profile.
"""

from __future__ import annotations

import logging
from collections import deque
from datetime import UTC, datetime
from typing import Any

from fastapi import FastAPI, Header, Request

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger("sc-mock")

MAX_HISTORY = 200
history: deque[dict[str, Any]] = deque(maxlen=MAX_HISTORY)

app = FastAPI(title="sensor.community mock")


@app.get("/")
async def index() -> dict[str, Any]:
    return {
        "service": "sensor.community mock",
        "received": len(history),
        "endpoints": {
            "push": "POST /v1/push-sensor-data/",
            "history": "GET /history",
            "reset": "POST /reset",
        },
    }


@app.post("/v1/push-sensor-data/")
async def push(
    request: Request,
    x_sensor: str | None = Header(default=None, alias="X-Sensor"),
    x_pin: str | None = Header(default=None, alias="X-Pin"),
) -> dict[str, str]:
    body = await request.json()
    entry = {
        "received_at": datetime.now(UTC).isoformat(),
        "sensor": x_sensor,
        "pin": x_pin,
        "body": body,
    }
    history.append(entry)
    logger.info("push sensor=%s pin=%s values=%s", x_sensor, x_pin, body.get("sensordatavalues"))
    return {"status": "ok"}


@app.get("/history")
async def get_history() -> dict[str, Any]:
    return {"count": len(history), "items": list(history)}


@app.post("/reset")
async def reset() -> dict[str, str]:
    history.clear()
    return {"status": "cleared"}
