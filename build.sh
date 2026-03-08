#!/bin/bash

set -euo pipefail

OS_ID=$(. /etc/os-release && echo "$ID")

if [ "$OS_ID" = "debian" ] || grep -qi "raspberry" /proc/device-tree/model 2>/dev/null; then
	PRESET="deploy"
	echo "==> Detected Raspberry Pi (os=$OS_ID), using deploy preset"
else
	PRESET="dev"
	echo "==> Detected dev machine (os=$OS_ID), using dev preset"
fi

echo "==> Configuring with preset: $PRESET"
cmake --preset "$PRESET"

echo "==> Building with preset: $PRESET"
cmake --build --preset "$PRESET" -j2

echo "==> Build complete"
