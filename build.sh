#!/bin/bash

set -euxo pipefail

FLIP_OVERRIDE=""
while [[ $# -gt 0 ]]; do
	case $1 in
		--flip)
			if [[ "$2" == "true" ]]; then
				FLIP_OVERRIDE="1"
			elif [[ "$2" == "false" ]]; then
				FLIP_OVERRIDE	="0"
			else
				echo "Usage: --flip true|false" >&2
				exit 1
			fi
			shift 2
			;;
		*)
			echo "Unknown option: $1" >&2
			exit 1
			;;
	esac
done

OS_ID=$(. /etc/os-release && echo "$ID")

if [ "$OS_ID" = "debian" ] || grep -qi "raspberry" /proc/device-tree/model 2>/dev/null; then
	PRESET="release"
	echo "==> Detected Raspberry Pi (os=$OS_ID), using release preset"
else
	PRESET="debug"
	echo "==> Detected dev machine (os=$OS_ID), using debug preset"
fi

EXTRA_FLAGS=""
if [[ -n "$FLIP_OVERRIDE" ]]; then
	EXTRA_FLAGS="-DDISPLAY_FLIP_OVERRIDE=$FLIP_OVERRIDE"
	echo "==> Flip override: DISPLAY_FLIP=$FLIP_OVERRIDE"
fi

echo "==> Configuring with preset: $PRESET"
cmake --preset "$PRESET" $EXTRA_FLAGS

echo "==> Building with preset: $PRESET"
cmake --build --preset "$PRESET" -j2

echo "==> Build complete"
