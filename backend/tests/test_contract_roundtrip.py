"""Smoke test: the example payload from openapi.yaml validates against the
generated Pydantic schema. Ensures the contract and the code stay in sync."""

from app.schemas import MeasurementPayload


def test_example_payload_validates() -> None:
    payload = {
        "message_id": "tbem-lora32-001-1744291920",
        "device": {
            "id": "tbem-lora32-001",
            "firmware_version": "1.0.0",
            "location": {"latitude": 43.2965, "longitude": 5.3698},
        },
        "timestamp": "2026-04-10T14:32:00Z",
        "transmission": {"protocol": "LoRa", "rssi": -87, "snr": 7.5},
        "battery": {"voltage_v": 3.85, "percentage": 78, "charging": False},
        "sensors": {
            "dust": {"type": "GP2Y1010AU0F", "P1": 45.2, "P2": 12.8},
            "bme280": {
                "type": "BMP280",
                "temperature": 22.4,
                "pressure": 1013.25,
                "humidity": None,
            },
        },
    }
    parsed = MeasurementPayload.model_validate(payload)
    assert parsed.device.id == "tbem-lora32-001"
    assert parsed.sensors.bme280.humidity is None
