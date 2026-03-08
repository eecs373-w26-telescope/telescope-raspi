#!/bin/bash
set -euo pipefail
echo "==> Stopping 373-telescope"
pkill -SIGTERM 373-telescope && echo "==> Stopped" || echo "==> Not running"
