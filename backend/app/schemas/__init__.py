"""Pydantic schemas.

`generated.py` is produced by `scripts/generate_schemas.sh` from `openapi.yaml`.
Do not edit it by hand - edit `openapi.yaml` and regenerate.

Hand-written helpers that wrap or extend the generated schemas can live
alongside this file (e.g. `pagination.py`).
"""

from app.schemas.generated import (
    Battery,
    BatteryHistory,
    BatteryHistoryEntry,
    Bme280Sensor,
    Device,
    DeviceInfo,
    DeviceList,
    DeviceUpdate,
    DustSensor,
    Error,
    HealthStatus,
    Location,
    Measurement,
    MeasurementAccepted,
    MeasurementList,
    MeasurementPayload,
    MicrophoneSensor,
    Sensors,
    SyncResult,
    Transmission,
)

__all__ = [
    "Battery",
    "BatteryHistory",
    "BatteryHistoryEntry",
    "Bme280Sensor",
    "Device",
    "DeviceInfo",
    "DeviceList",
    "DeviceUpdate",
    "DustSensor",
    "Error",
    "HealthStatus",
    "Location",
    "Measurement",
    "MeasurementAccepted",
    "MeasurementList",
    "MeasurementPayload",
    "MicrophoneSensor",
    "Sensors",
    "SyncResult",
    "Transmission",
]
