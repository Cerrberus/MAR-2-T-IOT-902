"""Fake gateway that POSTs realistic measurements to the API.

Simulates one or more LilyGo T-Beam devices behind a Heltec gateway:
- random walks around a baseline for temperature, pressure, PM values
- linear battery discharge (with occasional charging cycles)
- ISO-8601 timestamps, unique message_ids

Usage:
    python scripts/simulate_gateway.py --devices 2 --interval 5
    python scripts/simulate_gateway.py --once          # single shot
    python scripts/simulate_gateway.py --history 200   # backfill 200 past points

Requires the API to be running (default http://localhost:8000).
"""

from __future__ import annotations

import argparse
import asyncio
import random
import time
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone

import httpx


@dataclass
class DeviceState:
    id: str
    latitude: float
    longitude: float
    firmware_version: str = "1.0.0"
    battery_percentage: float = 100.0
    battery_charging: bool = False
    temperature: float = 21.0
    pressure: float = 1013.25
    humidity: float | None = 55.0
    p1: float = 20.0
    p2: float = 8.0
    msg_counter: int = field(default_factory=lambda: int(time.time()))

    def step(self) -> None:
        self.temperature += random.uniform(-0.3, 0.3)
        self.pressure += random.uniform(-0.2, 0.2)
        if self.humidity is not None:
            self.humidity = max(0.0, min(100.0, self.humidity + random.uniform(-1.0, 1.0)))
        self.p1 = max(0.0, self.p1 + random.uniform(-2.0, 2.5))
        self.p2 = max(0.0, self.p2 + random.uniform(-1.0, 1.2))

        if self.battery_charging:
            self.battery_percentage = min(100.0, self.battery_percentage + 1.5)
            if self.battery_percentage >= 100.0:
                self.battery_charging = False
        else:
            self.battery_percentage = max(0.0, self.battery_percentage - 0.05)
            if self.battery_percentage < 15.0 and random.random() < 0.1:
                self.battery_charging = True

    def payload(self, ts: datetime) -> dict:
        self.msg_counter += 1
        return {
            "message_id": f"{self.id}-{self.msg_counter}",
            "device": {
                "id": self.id,
                "firmware_version": self.firmware_version,
                "location": {"latitude": self.latitude, "longitude": self.longitude},
            },
            "timestamp": ts.astimezone(timezone.utc).isoformat().replace("+00:00", "Z"),
            "transmission": {
                "protocol": "LoRa",
                "rssi": random.randint(-110, -70),
                "snr": round(random.uniform(5.0, 10.0), 1),
            },
            "battery": {
                "voltage_v": round(3.2 + (self.battery_percentage / 100) * 0.9, 2),
                "percentage": int(self.battery_percentage),
                "charging": self.battery_charging,
            },
            "sensors": {
                "dust": {
                    "type": "GP2Y1010AU0F",
                    "P1": round(self.p1, 1),
                    "P2": round(self.p2, 1),
                },
                "bme280": {
                    "type": "BMP280",
                    "temperature": round(self.temperature, 1),
                    "pressure": round(self.pressure, 2),
                    "humidity": round(self.humidity, 1) if self.humidity is not None else None,
                },
            },
        }


def make_devices(n: int) -> list[DeviceState]:
    marseille = (43.2965, 5.3698)
    return [
        DeviceState(
            id=f"tbem-lora32-{i:03d}",
            latitude=marseille[0] + random.uniform(-0.05, 0.05),
            longitude=marseille[1] + random.uniform(-0.05, 0.05),
        )
        for i in range(1, n + 1)
    ]


async def send(client: httpx.AsyncClient, url: str, token: str, payload: dict) -> None:
    try:
        resp = await client.post(
            url,
            json=payload,
            headers={"Authorization": f"Bearer {token}"},
        )
        status = resp.status_code
        if status == 201:
            print(f"  ✓ {payload['device']['id']} @ {payload['timestamp']}")
        elif status == 409:
            print(f"  ⊘ duplicate {payload['message_id']}")
        else:
            print(f"  ✗ {status} {payload['device']['id']}: {resp.text}")
    except httpx.HTTPError as exc:
        print(f"  ✗ network error: {exc}")


async def run_backfill(
    client: httpx.AsyncClient,
    url: str,
    token: str,
    devices: list[DeviceState],
    count: int,
    step_seconds: int,
) -> None:
    start = datetime.now(timezone.utc) - timedelta(seconds=count * step_seconds)
    for i in range(count):
        ts = start + timedelta(seconds=i * step_seconds)
        for dev in devices:
            dev.step()
            await send(client, url, token, dev.payload(ts))


async def run_live(
    client: httpx.AsyncClient,
    url: str,
    token: str,
    devices: list[DeviceState],
    interval: float,
    iterations: int | None,
) -> None:
    i = 0
    while iterations is None or i < iterations:
        ts = datetime.now(timezone.utc)
        print(f"tick {i} @ {ts.isoformat()}")
        for dev in devices:
            dev.step()
            await send(client, url, token, dev.payload(ts))
        i += 1
        if iterations is None or i < iterations:
            await asyncio.sleep(interval)


async def amain(args: argparse.Namespace) -> None:
    devices = make_devices(args.devices)
    url = f"{args.base_url.rstrip('/')}/api/v1/measurements"
    async with httpx.AsyncClient(timeout=10.0) as client:
        if args.history:
            print(f"Backfilling {args.history} points per device ({args.devices} devices)...")
            await run_backfill(client, url, args.token, devices, args.history, args.interval)
            return
        iters = 1 if args.once else None
        print(f"Simulating {args.devices} device(s) every {args.interval}s → {url}")
        await run_live(client, url, args.token, devices, args.interval, iters)


def main() -> None:
    parser = argparse.ArgumentParser(description="Simulate a LoRa gateway for the Sensor Sensei API")
    parser.add_argument("--base-url", default="http://localhost:8000")
    parser.add_argument("--token", default="dev-token")
    parser.add_argument("--devices", type=int, default=1)
    parser.add_argument("--interval", type=float, default=5.0, help="seconds between batches")
    parser.add_argument("--once", action="store_true", help="send one batch then exit")
    parser.add_argument(
        "--history",
        type=int,
        default=0,
        help="backfill N past points per device (uses --interval as step)",
    )
    args = parser.parse_args()
    asyncio.run(amain(args))


if __name__ == "__main__":
    main()
