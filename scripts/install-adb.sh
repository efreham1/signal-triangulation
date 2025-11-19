#!/usr/bin/env bash
set -euo pipefail

# Installer for Android platform-tools (adb) on macOS, Linux, and WSL.
# - Detects WSL and uses apt
# - On macOS tries Homebrew, then download
# - On Linux tries apt, pacman, or download

OS="$(uname -s)"
IS_WSL=false
if grep -qi microsoft /proc/version 2>/dev/null || [ -f /proc/sys/fs/binfmt_misc/WSLInterop ]; then
    IS_WSL=true
fi

ensure_bin() {
    mkdir -p "$HOME/bin"
    if ! echo "$PATH" | tr ':' '\n' | grep -qx "$HOME/bin"; then
        echo "Note: '$HOME/bin' is not in PATH. Add 'export PATH=\$PATH:\$HOME/bin' to your shell profile."
    fi
}

install_via_brew() {
    echo "Installing android-platform-tools via Homebrew..."
    brew update || true
    brew install android-platform-tools
}

install_via_apt() {
    echo "Installing android-tools-adb via apt..."
    sudo apt-get update
    sudo apt-get install -y android-tools-adb
}

install_via_pacman() {
    echo "Installing android-tools via pacman..."
    sudo pacman -Sy --noconfirm android-tools
}

download_platform_tools() {
    echo "Downloading platform-tools..."
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "$tmpdir"' EXIT
    if [ "$OS" = "Darwin" ]; then
        url="https://dl.google.com/android/repository/platform-tools-latest-darwin.zip"
    else
        url="https://dl.google.com/android/repository/platform-tools-latest-linux.zip"
    fi
    zip="$tmpdir/platform-tools.zip"
    curl -L -o "$zip" "$url"
    unzip -q "$zip" -d "$tmpdir"
    PLAT="$HOME/platform-tools"
    rm -rf "$PLAT"
    mv "$tmpdir/platform-tools" "$PLAT"
    ensure_bin
    ln -sf "$PLAT/adb" "$HOME/bin/adb"
    ln -sf "$PLAT/fastboot" "$HOME/bin/fastboot" || true
    echo "platform-tools installed to: $PLAT"
    echo "Linked adb -> $HOME/bin/adb"
}

case "$OS" in
    Darwin)
        if command -v brew >/dev/null 2>&1; then
            install_via_brew
        else
            echo "Homebrew not found; falling back to downloading platform-tools."
            download_platform_tools
        fi
        ;;
    Linux)
        if [ "$IS_WSL" = true ]; then
            echo "WSL detected; using apt to install adb."
            install_via_apt
        elif command -v apt-get >/dev/null 2>&1; then
            install_via_apt
        elif command -v pacman >/dev/null 2>&1; then
            install_via_pacman
        else
            echo "No supported package manager found; falling back to downloading platform-tools."
            download_platform_tools
        fi
        ;;
    *)
        echo "Unsupported OS: $OS. This installer supports macOS, Linux, and WSL only." >&2
        exit 1
        ;;
esac

if command -v adb >/dev/null 2>&1; then
    echo "adb is now available at: $(command -v adb)"
else
    echo "adb was installed to ~/platform-tools and symlinked to ~/bin/adb (or instruct to add ~/bin to PATH)." 
fi

echo "Done."