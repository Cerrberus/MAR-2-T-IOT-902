"""Seed the database with known devices.

Run after migrations:
    python scripts/seed.py

Or via Docker:
    docker compose run --rm migrate  (already runs seed automatically)
"""

import asyncio
import os
from datetime import datetime, timedelta, timezone

from sqlalchemy import text
from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine

DATABASE_URL = os.environ["DATABASE_URL"]

ACTIVE_DEVICES = [
    {
        "id": "esp32-041f746cdda0",
        "firmware_version": "1.0.0",
        "latitude": 43.2965,
        "longitude": 5.3698,
        "sensor_community_id": 60634,
    },
    {
        "id": "esp32-6253",
        "firmware_version": "1.0.0",
        "latitude": 43.2965,
        "longitude": 5.3698,
        "sensor_community_id": 60874,
    },
]

# Capteurs inactifs : insérés une seule fois avec last_seen_at dans le passé
INACTIVE_DEVICES = [
    {
        "id": "tbem-lora32-offline",
        "firmware_version": "1.0.0",
        "latitude": 43.3147,
        "longitude": 5.4023,
        "sensor_community_id": None,
    },
]


async def seed() -> None:
    engine = create_async_engine(DATABASE_URL)
    async_session = async_sessionmaker(engine, expire_on_commit=False)

    last_seen_old = datetime.now(timezone.utc) - timedelta(days=45)

    async with async_session() as session:
        for device in ACTIVE_DEVICES:
            await session.execute(
                text("""
                    INSERT INTO devices (id, firmware_version, latitude, longitude, sensor_community_id)
                    VALUES (:id, :firmware_version, :latitude, :longitude, :sensor_community_id)
                    ON CONFLICT (id) DO UPDATE SET
                        sensor_community_id = EXCLUDED.sensor_community_id
                """),
                device,
            )

        for device in INACTIVE_DEVICES:
            await session.execute(
                text("""
                    INSERT INTO devices
                        (id, firmware_version, latitude, longitude, sensor_community_id,
                         first_seen_at, last_seen_at)
                    VALUES
                        (:id, :firmware_version, :latitude, :longitude, :sensor_community_id,
                         :ts, :ts)
                    ON CONFLICT (id) DO NOTHING
                """),
                {**device, "ts": last_seen_old},
            )

        await session.commit()
        total = len(ACTIVE_DEVICES) + len(INACTIVE_DEVICES)
        print(f"✓ seeded {total} device(s) ({len(INACTIVE_DEVICES)} inactif(s))")

    await engine.dispose()


if __name__ == "__main__":
    asyncio.run(seed())
