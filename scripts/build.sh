#!/bin/bash

set -euxo pipefail

PRESET="${1:-dev}"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" -j2
