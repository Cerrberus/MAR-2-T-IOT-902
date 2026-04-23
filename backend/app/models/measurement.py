from datetime import datetime

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Index,
    Integer,
    String,
    func,
)
from sqlalchemy.orm import Mapped, mapped_column

from app.db import Base


class Measurement(Base):
    __tablename__ = "measurements"
    __table_args__ = (
        Index("ix_measurements_device_timestamp", "device_id", "timestamp"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    message_id: Mapped[str] = mapped_column(String(128), unique=True, nullable=False)

    device_id: Mapped[str] = mapped_column(
        String(64), ForeignKey("devices.id", ondelete="CASCADE"), nullable=False
    )
    firmware_version: Mapped[str] = mapped_column(String(32), nullable=False)
    latitude: Mapped[float] = mapped_column(Float, nullable=False)
    longitude: Mapped[float] = mapped_column(Float, nullable=False)

    timestamp: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    received_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), nullable=False
    )

    tx_protocol: Mapped[str] = mapped_column(String(16), nullable=False)
    tx_rssi: Mapped[int] = mapped_column(Integer, nullable=False)
    tx_snr: Mapped[float] = mapped_column(Float, nullable=False)

    battery_voltage_v: Mapped[float] = mapped_column(Float, nullable=False)
    battery_percentage: Mapped[int] = mapped_column(Integer, nullable=False)
    battery_charging: Mapped[bool] = mapped_column(Boolean, nullable=False)

    sensors: Mapped[dict] = mapped_column(JSON, nullable=False)
