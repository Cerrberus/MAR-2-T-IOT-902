from pathlib import Path
from typing import Any

import yaml
from fastapi import FastAPI

from app.config import get_settings


def load_openapi_schema() -> dict[str, Any]:
    settings = get_settings()
    path = Path(settings.openapi_path)
    if not path.is_absolute():
        path = Path(__file__).resolve().parents[2] / settings.openapi_path
    with path.open("r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def install_openapi(app: FastAPI) -> None:
    """Serve the hand-written openapi.yaml as the app's schema (contract-first)."""
    schema = load_openapi_schema()

    def _custom_schema() -> dict[str, Any]:
        return schema

    app.openapi = _custom_schema  # type: ignore[assignment]
