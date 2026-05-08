#!/usr/bin/env bash
set -euo pipefail

# Cross compile for ARMv7 (no VFP — software float)
GOOS=linux GOARCH=arm GOARM=5 \
go build -ldflags="-s -w" -o webserver_armv5 .

echo "Built: webserver_armv5"
file webserver_armv5 2>/dev/null || true
ls -lh webserver_armv5
