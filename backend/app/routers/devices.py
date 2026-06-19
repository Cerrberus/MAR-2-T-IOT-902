from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.auth import require_gateway_token
from app.db import get_session
from app.models import Device as DeviceModel
from app.models import Measurement as MeasurementModel
from app.routers.measurements import _row_to_measurement
from app.schemas import Device, DeviceList, DeviceUpdate, Location, Measurement, SyncResult
from app.workers.sensor_community import forward as forward_to_sensor_community

router = APIRouter(prefix="/api/v1/devices", tags=["devices"])


async def _device_to_schema(session: AsyncSession, row: DeviceModel) -> Device:
    count = await session.scalar(
        select(func.count(MeasurementModel.id)).where(MeasurementModel.device_id == row.id)
    )
    return Device(
        id=row.id,
        firmware_version=row.firmware_version,
        location=Location(latitude=row.latitude, longitude=row.longitude),
        first_seen_at=row.first_seen_at,
        last_seen_at=row.last_seen_at,
        measurement_count=count or 0,
        sensor_community_id=row.sensor_community_id,
    )


@router.get("", response_model=DeviceList)
async def list_devices(
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> DeviceList:
    result = await session.execute(select(DeviceModel).order_by(DeviceModel.id))
    rows = result.scalars().all()
    items = [await _device_to_schema(session, r) for r in rows]
    return DeviceList(items=items)


@router.get("/{device_id}", response_model=Device)
async def get_device(
    device_id: str,
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> Device:
    row = await session.get(DeviceModel, device_id)
    if row is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail={"code": "device_not_found", "message": f"device {device_id} not found"},
        )
    return await _device_to_schema(session, row)


@router.get("/{device_id}/latest", response_model=Measurement)
async def get_latest_measurement(
    device_id: str,
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> Measurement:
    device = await session.get(DeviceModel, device_id)
    if device is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail={"code": "device_not_found", "message": f"device {device_id} not found"},
        )

    stmt = (
        select(MeasurementModel)
        .where(MeasurementModel.device_id == device_id)
        .order_by(MeasurementModel.timestamp.desc())
        .limit(1)
    )
    result = await session.execute(stmt)
    row = result.scalar_one_or_none()
    if row is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail={
                "code": "no_measurements",
                "message": f"device {device_id} has no measurements yet",
            },
        )
    return _row_to_measurement(row)


@router.post("/{device_id}/sync", response_model=SyncResult)
async def sync_to_sensor_community(
    device_id: str,
    from_: datetime | None = Query(default=None, alias="from"),
    to: datetime | None = Query(default=None),
    limit: int = Query(default=500, ge=1, le=5000),
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> SyncResult:
    device = await session.get(DeviceModel, device_id)
    if device is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail={"code": "device_not_found", "message": f"device {device_id} not found"},
        )
    if device.sensor_community_id is None:
        raise HTTPException(
            status_code=status.HTTP_422_UNPROCESSABLE_ENTITY,
            detail={
                "code": "no_sensor_community_id",
                "message": f"device {device_id} has no sensor_community_id set",
            },
        )

    stmt = (
        select(MeasurementModel)
        .where(MeasurementModel.device_id == device_id)
        .order_by(MeasurementModel.timestamp.asc())
    )
    if from_ is not None:
        stmt = stmt.where(MeasurementModel.timestamp >= from_)
    if to is not None:
        stmt = stmt.where(MeasurementModel.timestamp < to)
    stmt = stmt.limit(limit)

    result = await session.execute(stmt)
    rows = result.scalars().all()

    forwarded = 0
    skipped = 0
    for row in rows:
        measurement = _row_to_measurement(row)
        await forward_to_sensor_community(measurement, device.sensor_community_id)
        if measurement.sensors.bme280 is not None or measurement.sensors.dust is not None:
            forwarded += 1
        else:
            skipped += 1

    return SyncResult(device_id=device_id, forwarded=forwarded, skipped=skipped)


@router.patch("/{device_id}", response_model=Device)
async def update_device(
    device_id: str,
    payload: DeviceUpdate,
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> Device:
    row = await session.get(DeviceModel, device_id)
    if row is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail={"code": "device_not_found", "message": f"device {device_id} not found"},
        )
    fields = payload.model_dump(exclude_unset=True)
    if "sensor_community_id" in fields:
        row.sensor_community_id = fields["sensor_community_id"]
    await session.commit()
    await session.refresh(row)
    return await _device_to_schema(session, row)
