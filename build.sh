#!/bin/bash

set -euxo pipefail

OS_ID=$(. /etc/os-release && echo "$ID")

if [ "$OS_ID" = "debian" ] || grep -qi "raspberry" /proc/device-tree/model 2>/dev/null; then
	PRESET="release"
	echo "==> Detected Raspberry Pi (os=$OS_ID), using release preset"
else
	PRESET="debug"
	echo "==> Detected dev machine (os=$OS_ID), using debug preset"
fi

echo "==> Configuring with preset: $PRESET"
cmake --preset "$PRESET"

echo "==> Building with preset: $PRESET"
cmake --build --preset "$PRESET" -j2

echo "==> Build complete"
