from functools import lru_cache
from typing import Annotated

from pydantic import Field, field_validator
from pydantic_settings import BaseSettings, NoDecode, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", extra="ignore")

    database_url: str = Field(
        default="postgresql+asyncpg://sensei:sensei@localhost:5432/sensei",
        alias="DATABASE_URL",
    )
    log_level: str = Field(default="INFO", alias="LOG_LEVEL")

    gateway_tokens: Annotated[list[str], NoDecode] = Field(
        default_factory=lambda: ["dev-token"],
        alias="GATEWAY_TOKENS",
    )

    sensor_community_enabled: bool = Field(default=False, alias="SENSOR_COMMUNITY_ENABLED")
    sensor_community_url: str = Field(
        default="https://api.sensor.community/v1/push-sensor-data/",
        alias="SENSOR_COMMUNITY_URL",
    )

    openapi_path: str = Field(default="openapi.yaml", alias="OPENAPI_PATH")

    cors_origins: list[str] = Field(
        default_factory=lambda: ["http://localhost:5173"],
        alias="CORS_ORIGINS",
    )

    @field_validator("gateway_tokens", "cors_origins", mode="before")
    @classmethod
    def split_tokens(cls, v: str | list[str]) -> list[str]:
        if isinstance(v, str):
            return [t.strip() for t in v.split(",") if t.strip()]
        return v


@lru_cache
def get_settings() -> Settings:
    return Settings()
