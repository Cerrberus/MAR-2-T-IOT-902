from datetime import datetime

from fastapi import APIRouter, BackgroundTasks, Depends, HTTPException, Query, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.auth import require_gateway_token
from app.db import get_session
from app.models import Device as DeviceModel
from app.models import Measurement as MeasurementModel
from app.schemas import (
    Battery,
    Bme280Sensor,
    DeviceInfo,
    DustSensor,
    Location,
    Measurement,
    MeasurementAccepted,
    MeasurementList,
    MeasurementPayload,
    MicrophoneSensor,
    Sensors,
    Transmission,
)
from app.workers.sensor_community import forward as forward_to_sensor_community

router = APIRouter(prefix="/api/v1/measurements", tags=["measurements"])


def _row_to_measurement(row: MeasurementModel) -> Measurement:
    return Measurement(
        message_id=row.message_id,
        device=DeviceInfo(
            id=row.device_id,
            firmware_version=row.firmware_version,
            location=Location(latitude=row.latitude, longitude=row.longitude),
        ),
        timestamp=row.timestamp,
        transmission=Transmission(
            protocol=row.tx_protocol,  # type: ignore[arg-type]
            rssi=row.tx_rssi,
            snr=row.tx_snr,
        ),
        battery=Battery(
            voltage_v=row.battery_voltage_v,
            percentage=row.battery_percentage,
            charging=row.battery_charging,
        ),
        sensors=Sensors(
            dust=DustSensor(**row.sensors["dust"]) if row.sensors.get("dust") else None,
            bme280=Bme280Sensor(**row.sensors["bme280"]),
            microphone=MicrophoneSensor(**row.sensors["microphone"]) if row.sensors.get("microphone") else None,
        ),
        received_at=row.received_at,
    )


@router.post(
    "",
    response_model=MeasurementAccepted,
    status_code=status.HTTP_201_CREATED,
)
async def ingest_measurement(
    payload: MeasurementPayload,
    background_tasks: BackgroundTasks,
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> MeasurementAccepted:
    device = await session.get(DeviceModel, payload.device.id)
    if device is None:
        device = DeviceModel(
            id=payload.device.id,
            firmware_version=payload.device.firmware_version,
            latitude=payload.device.location.latitude,
            longitude=payload.device.location.longitude,
        )
        session.add(device)
    else:
        device.firmware_version = payload.device.firmware_version
        device.latitude = payload.device.location.latitude
        device.longitude = payload.device.location.longitude
        device.last_seen_at = payload.timestamp

    measurement = MeasurementModel(
        message_id=payload.message_id,
        device_id=payload.device.id,
        firmware_version=payload.device.firmware_version,
        latitude=payload.device.location.latitude,
        longitude=payload.device.location.longitude,
        timestamp=payload.timestamp,
        tx_protocol=payload.transmission.protocol,
        tx_rssi=payload.transmission.rssi,
        tx_snr=payload.transmission.snr,
        battery_voltage_v=payload.battery.voltage_v,
        battery_percentage=payload.battery.percentage,
        battery_charging=payload.battery.charging,
        sensors=payload.sensors.model_dump(),
    )
    session.add(measurement)

    try:
        await session.commit()
    except IntegrityError:
        await session.rollback()
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail={
                "code": "duplicate_message",
                "message": f"message_id {payload.message_id} already ingested",
            },
        ) from None

    await session.refresh(measurement)
    await session.refresh(device)

    background_tasks.add_task(
        forward_to_sensor_community, payload, device.sensor_community_id
    )

    return MeasurementAccepted(
        message_id=measurement.message_id,
        received_at=measurement.received_at,
    )


@router.get("", response_model=MeasurementList)
async def list_measurements(
    device_id: str | None = Query(default=None),
    from_: datetime | None = Query(default=None, alias="from"),
    to: datetime | None = Query(default=None),
    limit: int = Query(default=100, ge=1, le=1000),
    cursor: str | None = Query(default=None),
    session: AsyncSession = Depends(get_session),
    _token: str = Depends(require_gateway_token),
) -> MeasurementList:
    stmt = select(MeasurementModel).order_by(MeasurementModel.timestamp.desc())
    if device_id is not None:
        stmt = stmt.where(MeasurementModel.device_id == device_id)
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
    rows = result.scalars().all()

    next_cursor = None
    if len(rows) > limit:
        next_cursor = str(rows[limit - 1].id)
        rows = rows[:limit]

    return MeasurementList(
        items=[_row_to_measurement(r) for r in rows],
        next_cursor=next_cursor,
    )
