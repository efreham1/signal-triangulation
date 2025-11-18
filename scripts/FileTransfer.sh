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

ADB_CMD="adb"
if [ "$IS_WSL" = true ]; then
  WIN_USER="$(cmd.exe /c 'echo %USERNAME%' 2>/dev/null | tr -d '\r' || true)"
  if [ -n "$WIN_USER" ]; then
    PT="/mnt/c/Users/$WIN_USER/AppData/Local/Android/Sdk/platform-tools/adb.exe"
    if [ -f "$PT" ] || [ -x "$PT" ]; then
      ADB_CMD="$PT"
      echo "(WSL detected; using adb.exe at $PT)"
    fi
  fi
fi

echo "Listing connected adb devices:"
$ADB_CMD devices -l

device_line=$($ADB_CMD devices | tr -d '\r' | awk 'NR>1 && $2=="device" { print $1; exit }') || true
if [ -z "${device_line:-}" ]; then
  echo "No connected adb device in 'device' state found. Raw list:" >&2
  $ADB_CMD devices | tr -d '\r' >&2
  exit 2
fi

echo "Pulling from device '$device_line': $REMOTE_PATH"

if [ "$IS_WSL" = true ] && [[ "$ADB_CMD" == *adb.exe ]]; then
  if command -v wslpath >/dev/null 2>&1; then
    DEST_DIR_WIN=$(wslpath -w "$DEST_DIR")
  else
    # Fallback manual conversion (may fail if path unusual)
    DEST_DIR_WIN="C:$(echo "$DEST_DIR" | sed -E 's|/mnt/c||' | tr '/' '\\')"
  fi
  echo "(WSL/Windows adb.exe) target (Windows path): $DEST_DIR_WIN"
  mkdir -p "$DEST_DIR"  # ensure WSL side exists
  "$ADB_CMD" -s "$device_line" pull "$REMOTE_PATH" "$DEST_DIR_WIN"
else
  $ADB_CMD -s "$device_line" pull "$REMOTE_PATH" "$DEST_DIR"
fi
pull_rc=$?
if [ $pull_rc -ne 0 ]; then
  echo "adb pull failed (exit code $pull_rc)." >&2
  exit $pull_rc
fi

echo "adb pull completed. Files written to: $DEST_DIR"
exit 0