#!/bin/bash
# filepath: /mnt/c/Users/Lokalt/StudioProjects/signal-triangulation/scripts/start_server.sh

set -e

PORT=${1:-8080}
OUTPUT_DIR=${2:-uploads}

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=== Polaris Server Startup ==="

# Setup WSL port forwarding (if in WSL)
if grep -qi microsoft /proc/version 2>/dev/null; then
    echo "[1/2] Setting up WSL port forwarding..."
    WSL_IP=$(hostname -I | awk '{print $1}')
    powershell.exe -Command "Start-Process powershell -ArgumentList '-ExecutionPolicy Bypass -Command \`\"netsh interface portproxy delete v4tov4 listenport=$PORT listenaddress=0.0.0.0 2>\`\$null; netsh interface portproxy add v4tov4 listenport=$PORT listenaddress=0.0.0.0 connectport=$PORT connectaddress=$WSL_IP; Write-Host Port $PORT forwarded to $WSL_IP\`\"' -Verb RunAs" 2>/dev/null || echo "  (Run as admin manually if needed)"
else
    echo "[1/2] Not in WSL, skipping port forward"
fi

# Start the file receiver
echo "[2/2] Starting FileReceiver on port $PORT..."
echo ""

# Run the server
"$PROJECT_DIR/build/file-receiver" --port "$PORT" --output "$OUTPUT_DIR"