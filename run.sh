#!/bin/bash
set -euxo pipefail

MOCK=false
while [[ $# -gt 0 ]]; do
	case $1 in
		--mock) MOCK=true; shift ;;
		*) echo "Unknown option: $1" >&2; exit 1 ;;
	esac
done

SOCAT_PID=""
MOCK_PID=""

cleanup() {
	[ -n "$MOCK_PID" ]  && kill "$MOCK_PID"  2>/dev/null || true
	[ -n "$SOCAT_PID" ] && kill "$SOCAT_PID" 2>/dev/null || true
	rm -f /tmp/mock_app /tmp/mock_sender
	echo "==> Stopped"
	exit
}
trap cleanup INT TERM

if $MOCK; then
	echo "==> Mock mode: starting virtual serial pair via socat"
	socat pty,raw,echo=0,link=/tmp/mock_app pty,raw,echo=0,link=/tmp/mock_sender &
	SOCAT_PID=$!
	for i in $(seq 10); do
		[ -e /tmp/mock_app ] && [ -e /tmp/mock_sender ] && break
		sleep 0.1
	done
	if [ ! -e /tmp/mock_app ]; then
		echo "ERROR: socat failed to create virtual serial ports" >&2
		exit 1
	fi
	SERIAL_DEV="/tmp/mock_app"
	echo "==> Virtual ports ready: app=$SERIAL_DEV  sender=/tmp/mock_sender"

	python3 "$(dirname "$0")/tools/mock_sender.py" &
	MOCK_PID=$!
	echo "==> Mock sender started (pid=$MOCK_PID)"
else
	SERIAL_DEV="/dev/ttyS0"
	echo "==> Using $SERIAL_DEV"
fi

./build/373-telescope "$SERIAL_DEV" &
APP_PID=$!
echo "==> Started 373-telescope (pid=$APP_PID)"

wait "$APP_PID"
cleanup
