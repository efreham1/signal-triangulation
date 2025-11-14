#!/usr/bin/env bash
set -euo pipefail
# Wrapper to preserve compatibility — the real script moved to ./scripts/FileTransfer.sh
exec "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../scripts/FileTransfer.sh"
