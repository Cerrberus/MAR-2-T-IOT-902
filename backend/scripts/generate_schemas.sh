#!/usr/bin/env bash
# Regenerate Pydantic schemas from openapi.yaml (contract-first).
#
# Usage:
#   ./scripts/generate_schemas.sh
#
# Requires: datamodel-code-generator (installed via the [dev] extras).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

OUTPUT="app/schemas/generated.py"

datamodel-codegen \
  --input openapi.yaml \
  --input-file-type openapi \
  --output "$OUTPUT" \
  --output-model-type pydantic_v2.BaseModel \
  --target-python-version 3.12 \
  --use-standard-collections \
  --use-union-operator \
  --use-schema-description \
  --use-field-description \
  --field-constraints \
  --snake-case-field \
  --disable-timestamp \
  --enum-field-as-literal all

echo "✓ regenerated $OUTPUT"
