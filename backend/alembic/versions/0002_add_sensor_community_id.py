"""add sensor_community_id to devices

Revision ID: 0002_add_sensor_community_id
Revises: 0001_initial
Create Date: 2026-04-23
"""
from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "0002_add_sensor_community_id"
down_revision: str | None = "0001_initial"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column(
        "devices",
        sa.Column("sensor_community_id", sa.Integer(), nullable=True),
    )


def downgrade() -> None:
    op.drop_column("devices", "sensor_community_id")
