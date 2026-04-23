"""Placeholder worker for forwarding measurements to sensor.community.

Not wired into the ingestion path yet — intentionally a no-op until the
external push format is finalised. Enabling `SENSOR_COMMUNITY_ENABLED=true`
will flip the guard in `forward()` on.
"""

from __future__ import annotations

import logging

import httpx

from app.config import get_settings
from app.schemas import MeasurementPayload

logger = logging.getLogger(__name__)


async def forward(payload: MeasurementPayload) -> None:
    settings = get_settings()
    if not settings.sensor_community_enabled:
        return

    body = {
        "software_version": "sensor-sensei/1.0",
        "sensordatavalues": [
            {"value_type": "P1", "value": payload.sensors.dust.P1},
            {"value_type": "P2", "value": payload.sensors.dust.P2},
            {"value_type": "temperature", "value": payload.sensors.bme280.temperature},
            {"value_type": "pressure", "value": payload.sensors.bme280.pressure},
        ],
    }
    if payload.sensors.bme280.humidity is not None:
        body["sensordatavalues"].append(
            {"value_type": "humidity", "value": payload.sensors.bme280.humidity}
        )

    async with httpx.AsyncClient(timeout=10.0) as client:
        try:
            resp = await client.post(
                settings.sensor_community_url,
                json=body,
                headers={
                    "X-Sensor": payload.device.id,
                    "X-Pin": "1",
                },
            )
            resp.raise_for_status()
        except httpx.HTTPError as exc:
            logger.warning("sensor.community forward failed: %s", exc)
