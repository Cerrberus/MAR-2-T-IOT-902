"""initial schema

Revision ID: 0001_initial
Revises:
Create Date: 2026-04-23
"""
from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "0001_initial"
down_revision: str | None = None
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.create_table(
        "devices",
        sa.Column("id", sa.String(length=64), primary_key=True),
        sa.Column("firmware_version", sa.String(length=32), nullable=False),
        sa.Column("latitude", sa.Float(), nullable=False),
        sa.Column("longitude", sa.Float(), nullable=False),
        sa.Column(
            "first_seen_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
        sa.Column(
            "last_seen_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
    )

    op.create_table(
        "measurements",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("message_id", sa.String(length=128), nullable=False, unique=True),
        sa.Column(
            "device_id",
            sa.String(length=64),
            sa.ForeignKey("devices.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("firmware_version", sa.String(length=32), nullable=False),
        sa.Column("latitude", sa.Float(), nullable=False),
        sa.Column("longitude", sa.Float(), nullable=False),
        sa.Column("timestamp", sa.DateTime(timezone=True), nullable=False),
        sa.Column(
            "received_at",
            sa.DateTime(timezone=True),
            server_default=sa.func.now(),
            nullable=False,
        ),
        sa.Column("tx_protocol", sa.String(length=16), nullable=False),
        sa.Column("tx_rssi", sa.Integer(), nullable=False),
        sa.Column("tx_snr", sa.Float(), nullable=False),
        sa.Column("battery_voltage_v", sa.Float(), nullable=False),
        sa.Column("battery_percentage", sa.Integer(), nullable=False),
        sa.Column("battery_charging", sa.Boolean(), nullable=False),
        sa.Column("sensors", sa.JSON(), nullable=False),
    )
    op.create_index(
        "ix_measurements_device_timestamp",
        "measurements",
        ["device_id", "timestamp"],
    )


def downgrade() -> None:
    op.drop_index("ix_measurements_device_timestamp", table_name="measurements")
    op.drop_table("measurements")
    op.drop_table("devices")
