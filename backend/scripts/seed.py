"""Seed the database with known devices.

Run after migrations:
    python scripts/seed.py

Or via Docker:
    docker compose run --rm migrate  (already runs seed automatically)
"""

import asyncio
import os

from sqlalchemy import text
from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine

DATABASE_URL = os.environ["DATABASE_URL"]

DEVICES = [
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


async def seed() -> None:
    engine = create_async_engine(DATABASE_URL)
    async_session = async_sessionmaker(engine, expire_on_commit=False)

    async with async_session() as session:
        for device in DEVICES:
            await session.execute(
                text("""
                    INSERT INTO devices (id, firmware_version, latitude, longitude, sensor_community_id)
                    VALUES (:id, :firmware_version, :latitude, :longitude, :sensor_community_id)
                    ON CONFLICT (id) DO UPDATE SET
                        sensor_community_id = EXCLUDED.sensor_community_id
                """),
                device,
            )
        await session.commit()
        print(f"✓ seeded {len(DEVICES)} device(s)")

    await engine.dispose()


if __name__ == "__main__":
    asyncio.run(seed())
