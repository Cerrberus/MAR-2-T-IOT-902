from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.auth import require_gateway_token
from app.db import get_session
from app.models import Device as DeviceModel
from app.models import Measurement as MeasurementModel
from app.schemas import BatteryHistory, BatteryHistoryEntry

router = APIRouter(prefix="/api/v1/devices", tags=["battery"])


@router.get("/{device_id}/battery/history", response_model=BatteryHistory)
async def get_battery_history(
    device_id: str,
    from_: datetime | None = Query(default=None, alias="from"),
    to: datetime | None = Query(default=None),
    limit: int = Query(default=100, ge=1, le=1000),
    cursor: str | None = Query(default=None),
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> BatteryHistory:
    device = await session.get(DeviceModel, device_id)
    if device is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail={"code": "device_not_found", "message": f"device {device_id} not found"},
        )

    stmt = (
        select(
            MeasurementModel.id,
            MeasurementModel.timestamp,
            MeasurementModel.battery_voltage_v,
            MeasurementModel.battery_percentage,
            MeasurementModel.battery_charging,
        )
        .where(MeasurementModel.device_id == device_id)
        .order_by(MeasurementModel.timestamp.desc())
    )
    if from_ is not None:
        stmt = stmt.where(MeasurementModel.timestamp >= from_)
    if to is not None:
        stmt = stmt.where(MeasurementModel.timestamp < to)
    if cursor is not None:
        try:
            cursor_id = int(cursor)
        except ValueError:
            raise HTTPException(
                status_code=400,
                detail={"code": "invalid_cursor", "message": "cursor must be opaque id"},
            ) from None
        stmt = stmt.where(MeasurementModel.id < cursor_id)

    stmt = stmt.limit(limit + 1)
    result = await session.execute(stmt)
    rows = result.all()

    next_cursor = None
    if len(rows) > limit:
        next_cursor = str(rows[limit - 1].id)
        rows = rows[:limit]

    items = [
        BatteryHistoryEntry(
            timestamp=r.timestamp,
            voltage_v=r.battery_voltage_v,
            percentage=r.battery_percentage,
            charging=r.battery_charging,
        )
        for r in rows
    ]
    return BatteryHistory(device_id=device_id, items=items, next_cursor=next_cursor)
