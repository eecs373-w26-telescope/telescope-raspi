#!/bin/bash

set -euxo pipefail

PRESET="${1:-dev}"

echo "==> Configuring with preset: $PRESET"
cmake --preset "$PRESET"

echo "==> Building with preset: $PRESET"
cmake --build --preset "$PRESET" -j2

echo "==> Build complete"
