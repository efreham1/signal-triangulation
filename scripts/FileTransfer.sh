#!/usr/bin/env bash
set -euo pipefail

# Pull the Android app `files` folder into the repository (Linux, macOS, and WSL).

REMOTE_PATH="/sdcard/Android/data/com.example.polaris/files"

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
DEST_DIR="$REPO_ROOT/Recordings"
mkdir -p "$DEST_DIR"

# Detect WSL
IS_WSL=false
if grep -qi microsoft /proc/version 2>/dev/null || [ -f /proc/sys/fs/binfmt_misc/WSLInterop ]; then
    IS_WSL=true
fi

if ! command -v adb >/dev/null 2>&1; then
  echo "adb not found on PATH. Run 'make install-adb' or install Android platform-tools and ensure 'adb' is on PATH." >&2
  exit 1
fi

# On WSL, prefer adb.exe from Windows if available (Linux adb won't see devices)
ADB_CMD="adb"
if [ "$IS_WSL" = true ]; then
  if command -v adb.exe >/dev/null 2>&1; then
    ADB_CMD="adb.exe"
    echo "(WSL detected; using adb.exe from Windows)"
  fi
fi

device_line=$($ADB_CMD devices | awk 'NR>1 && $2=="device" { print $1; exit }') || true
if [ -z "$device_line" ]; then
  echo "No connected adb device in 'device' state found. Run '$ADB_CMD devices' to check." >&2
  exit 2
fi

echo "Pulling from device '$device_line': $REMOTE_PATH -> $DEST_DIR"

# On WSL, if adb.exe is available on the Windows path, use it
if [ "$IS_WSL" = true ] && command -v adb.exe >/dev/null 2>&1; then
  echo "(WSL detected; using adb.exe from Windows)"
  adb.exe -s "$device_line" pull "$REMOTE_PATH" "$DEST_DIR"
else
  $ADB_CMD -s "$device_line" pull "$REMOTE_PATH" "$DEST_DIR"
fi
pull_rc=$?
if [ $pull_rc -ne 0 ]; then
  echo "adb pull failed (exit code $pull_rc). Check device connection and permissions." >&2
  exit $pull_rc
fi

echo "adb pull completed. Files written to: $DEST_DIR"
exit 0
