import logging

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from app.config import get_settings
from app.core.errors import register_exception_handlers
from app.core.openapi import install_openapi
from app.routers import battery, devices, health, measurements

settings = get_settings()
logging.basicConfig(level=settings.log_level)

app = FastAPI(
    title="Sensor Sensei API",
    version="1.0.0",
    docs_url="/docs",
    redoc_url="/redoc",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_methods=["GET", "POST", "PATCH", "OPTIONS"],
    allow_headers=["Authorization", "Content-Type"],
)

register_exception_handlers(app)
install_openapi(app)

app.include_router(health.router)
app.include_router(measurements.router)
app.include_router(devices.router)
app.include_router(battery.router)
