"""GENERATED FILE — do not edit by hand.

Regenerate with `./scripts/generate_schemas.sh` after updating `openapi.yaml`.

This file is checked in so the project runs without requiring the generation
toolchain. CI should fail if `generated.py` drifts from `openapi.yaml`.
"""

from __future__ import annotations

from datetime import datetime
from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field


class _Model(BaseModel):
    model_config = ConfigDict(populate_by_name=True, extra="forbid")


class Error(_Model):
    code: str
    message: str
    details: dict[str, Any] | None = None


class HealthStatus(_Model):
    status: Literal["ok", "degraded"]
    database: Literal["ok", "down"]
    version: str | None = None


class Location(_Model):
    latitude: float = Field(ge=-90, le=90)
    longitude: float = Field(ge=-180, le=180)


class DeviceInfo(_Model):
    id: str = Field(pattern=r"^[a-zA-Z0-9-_]{1,64}$")
    firmware_version: str
    location: Location


class Device(_Model):
    id: str
    firmware_version: str
    location: Location
    first_seen_at: datetime
    last_seen_at: datetime
    measurement_count: int | None = Field(default=None, ge=0)


class DeviceList(_Model):
    items: list[Device]


class Transmission(_Model):
    protocol: Literal["LoRa", "WiFi", "BLE"]
    rssi: int
    snr: float


class Battery(_Model):
    voltage_v: float = Field(ge=0, le=5)
    percentage: int = Field(ge=0, le=100)
    charging: bool


class DustSensor(_Model):
    type: str
    P1: float = Field(ge=0)
    P2: float = Field(ge=0)


class Bme280Sensor(_Model):
    type: str
    temperature: float
    pressure: float
    humidity: float | None = Field(default=None, ge=0, le=100)


class Sensors(_Model):
    dust: DustSensor
    bme280: Bme280Sensor


class MeasurementPayload(_Model):
    message_id: str
    device: DeviceInfo
    timestamp: datetime
    transmission: Transmission
    battery: Battery
    sensors: Sensors


class Measurement(MeasurementPayload):
    received_at: datetime


class MeasurementAccepted(_Model):
    message_id: str
    received_at: datetime


class MeasurementList(_Model):
    items: list[Measurement]
    next_cursor: str | None = None


class BatteryHistoryEntry(_Model):
    timestamp: datetime
    voltage_v: float
    percentage: int = Field(ge=0, le=100)
    charging: bool


class BatteryHistory(_Model):
    device_id: str
    items: list[BatteryHistoryEntry]
    next_cursor: str | None = None
