#!/usr/bin/env bash
set -euo pipefail

# Pull the Android app `files` folder into the repository (Linux / macOS only).

REMOTE_PATH="/sdcard/Android/data/com.example.polaris/files"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
DEST_DIR="$REPO_ROOT/recordings"
mkdir -p "$DEST_DIR"

if ! command -v adb >/dev/null 2>&1; then
  echo "adb not found on PATH. Run 'make install-adb' or install Android platform-tools and ensure 'adb' is on PATH." >&2
  exit 1
fi

device_line=$(adb devices | awk 'NR>1 && $2=="device" { print $1; exit }') || true
if [ -z "$device_line" ]; then
  echo "No connected adb device in 'device' state found. Run 'adb devices' to check." >&2
  exit 2
fi

echo "Pulling from device '$device_line': $REMOTE_PATH -> $DEST_DIR"

adb -s "$device_line" pull "$REMOTE_PATH" "$DEST_DIR"
pull_rc=$?
if [ $pull_rc -ne 0 ]; then
  echo "adb pull failed (exit code $pull_rc). Check device connection and permissions." >&2
  exit $pull_rc
fi

echo "adb pull completed. Files written to: $DEST_DIR"
exit 0
