#!/bin/bash
# filepath: /mnt/c/Users/Lokalt/StudioProjects/signal-triangulation/scripts/start_server.sh

set -e

PORT=${1:-8080}
OUTPUT_DIR=${2:-uploads}

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Only set Windows script paths if in WSL
if grep -qi microsoft /proc/version 2>/dev/null; then
    WINDOWS_SCRIPT_PATH=$(wslpath -w "$SCRIPT_DIR/setup_port_forward.ps1")
    WINDOWS_CLEANUP_PATH=$(wslpath -w "$SCRIPT_DIR/cleanup_port_forward.ps1")
fi

cleanup() {
    if grep -qi microsoft /proc/version 2>/dev/null; then
        echo "Cleaning up portproxy and firewall rule..."
        powershell.exe -Command "Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy Bypass -File \"$WINDOWS_CLEANUP_PATH\" -Port $PORT'"
    fi
}
trap cleanup EXIT

echo "=== Polaris Server Startup ==="

# Setup WSL port forwarding (if in WSL)
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "[1/2] Setting up WSL port forwarding using PowerShell script..."
    WSL_IP=$(hostname -I | awk '{print $1}')
    powershell.exe -Command "Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy Bypass -File \"$WINDOWS_SCRIPT_PATH\" -Port $PORT -WSL_IP $WSL_IP'"
else
    echo "[1/2] Not in WSL, skipping port forward"
fi

# Start the REST API server
echo "[2/2] Starting REST API server on port $PORT..."
echo ""

"$PROJECT_DIR/build/rest-api-server" --port "$PORT" --output "$OUTPUT_DIR"