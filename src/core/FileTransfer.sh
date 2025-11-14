#!/usr/bin/env bash
set -euo pipefail

# POSIX shell script to pull the Android app `files` folder into the repository.
#
# Run from repo root:
#   ./src/core/FileTransfer.sh
# or:
#   bash ./src/core/FileTransfer.sh

REMOTE_PATH="/sdcard/Android/data/com.example.polaris/files"

# Determine script and repo root
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
DEST_DIR="$REPO_ROOT/Recordings"
FINAL_DEST="$DEST_DIR"
mkdir -p "$DEST_DIR"

if ! command -v adb >/dev/null 2>&1; then
	echo "adb not found on PATH. Install Android platform-tools and ensure 'adb' is on PATH." >&2
	exit 1
fi

# Select first connected device in 'device' state
device_line=$(adb devices | awk 'NR>1 && $2=="device" { print $1; exit }') || true
if [ -z "$device_line" ]; then
	echo "No connected adb device in 'device' state found. Run 'adb devices' to check." >&2
	exit 2
fi

echo "Pulling from device '$device_line': $REMOTE_PATH -> $FINAL_DEST"

if [ -n "${MSYSTEM-}" ] || (uname 2>/dev/null | grep -qi mingw); then
	if command -v cygpath >/dev/null 2>&1; then
		local_dest=$(cygpath -w "$FINAL_DEST")
	else
		local_dest="$FINAL_DEST"
	fi
	MSYS_NO_PATHCONV=1 adb -s "$device_line" pull "$REMOTE_PATH" "$local_dest"
else
	adb -s "$device_line" pull "$REMOTE_PATH" "$FINAL_DEST"
fi

pull_rc=$?
if [ $pull_rc -ne 0 ]; then
	echo "adb pull failed (exit code $pull_rc). Check device connection and permissions." >&2
	exit $pull_rc
fi

echo "adb pull completed. Files written to: $FINAL_DEST"
exit 0