#!/bin/bash
set -euo pipefail

./build/373-telescope &
PID=$!
echo "==> Started 373-telescope (pid=$PID)"

trap "kill $PID 2>/dev/null; wait $PID 2>/dev/null; echo '==> Stopped'; exit" INT TERM
wait $PID
