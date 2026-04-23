from fastapi import APIRouter, Depends
from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncSession

from app import __version__
from app.db import get_session
from app.schemas import HealthStatus

router = APIRouter(prefix="/api/v1", tags=["system"])


@router.get("/health", response_model=HealthStatus)
async def get_health(session: AsyncSession = Depends(get_session)) -> HealthStatus:
    database = "ok"
    try:
        await session.execute(text("SELECT 1"))
    except Exception:
        database = "down"
    return HealthStatus(
        status="ok" if database == "ok" else "degraded",
        database=database,
        version=__version__,
    )
