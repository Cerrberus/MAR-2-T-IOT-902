"""Forward a measurement to sensor.community.

sensor.community expects one POST per sensor "module". For each measurement
we emit:
  - Pin 1  → PM values (P1, P2) from the GP2Y dust sensor
  - Pin 11 → BME280 values (temperature, pressure, humidity)

Calls are best-effort: failures are logged but never re-raised (this runs
as a FastAPI BackgroundTask after the HTTP response has been sent).
"""

from __future__ import annotations

import logging

import httpx

from app.config import get_settings
from app.schemas import MeasurementPayload

logger = logging.getLogger(__name__)

PIN_DUST = 1
PIN_BME280 = 11
SOFTWARE_VERSION = "sensor-sensei/1.0"


def _post_body(values: list[dict[str, str | float]]) -> dict:
    return {"software_version": SOFTWARE_VERSION, "sensordatavalues": values}


def _dust_values(payload: MeasurementPayload) -> list[dict[str, str | float]]:
    return [
        {"value_type": "P1", "value": str(payload.sensors.dust.P1)},
        {"value_type": "P2", "value": str(payload.sensors.dust.P2)},
    ]


def _bme280_values(payload: MeasurementPayload) -> list[dict[str, str | float]]:
    values: list[dict[str, str | float]] = [
        {"value_type": "temperature", "value": str(payload.sensors.bme280.temperature)},
        {"value_type": "pressure", "value": str(payload.sensors.bme280.pressure)},
    ]
    if payload.sensors.bme280.humidity is not None:
        values.append(
            {"value_type": "humidity", "value": str(payload.sensors.bme280.humidity)}
        )
    return values


async def _post(
    client: httpx.AsyncClient,
    url: str,
    sensor_community_id: int,
    pin: int,
    body: dict,
) -> None:
    try:
        resp = await client.post(
            url,
            json=body,
            headers={
                "X-Sensor": str(sensor_community_id),
                "X-Pin": str(pin),
                "Content-Type": "application/json",
            },
        )
        resp.raise_for_status()
        logger.info(
            "sensor.community push ok sensor=%s pin=%s status=%s",
            sensor_community_id,
            pin,
            resp.status_code,
        )
    except httpx.HTTPError as exc:
        logger.warning(
            "sensor.community push failed sensor=%s pin=%s error=%s",
            sensor_community_id,
            pin,
            exc,
        )


async def forward(payload: MeasurementPayload, sensor_community_id: int | None) -> None:
    settings = get_settings()
    if not settings.sensor_community_enabled:
        return
    if sensor_community_id is None:
        logger.debug(
            "skipping forward for device %s: no sensor_community_id set",
            payload.device.id,
        )
        return

    url = settings.sensor_community_url
    async with httpx.AsyncClient(timeout=10.0) as client:
        if payload.sensors.dust is not None:
            await _post(client, url, sensor_community_id, PIN_DUST, _post_body(_dust_values(payload)))
        await _post(
            client, url, sensor_community_id, PIN_BME280, _post_body(_bme280_values(payload))
        )
