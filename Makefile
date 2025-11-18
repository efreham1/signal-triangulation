# Simple Makefile wrapper that configures and builds with CMake
# Usage:
#   make          # Configure (cmake -S . -B build) and build (cmake --build build)
#   make clean    # Remove build directory
#   make rebuild  # Clean, configure and build

BUILD_DIR := build
CMAKE := cmake
CMAKE_BUILD := $(CMAKE) --build $(BUILD_DIR) --config Release
CMAKE_FLAGS := -S . -B $(BUILD_DIR)

.PHONY: all clean rebuild configure build

all: configure build

.PHONY: install-adb fetch_recordings

install-adb:
	@echo "Detecting platform and installing adb if needed..."
	@if [ "$$(uname -s)" = "Darwin" ] || [ "$$(uname -s)" = "Linux" ]; then \
		if command -v adb >/dev/null 2>&1; then \
			echo "adb already installed: $$(command -v adb)"; \
		else \
			bash ./scripts/install-adb.sh; \
		fi; \
	else \
		powershell -NoProfile -Command "if (Get-Command adb -ErrorAction SilentlyContinue) { Write-Host 'adb already installed'; exit 0 } else { Write-Host 'Installing adb...'; powershell -ExecutionPolicy Bypass -File ./scripts/install-adb.ps1; exit $$LASTEXITCODE }"; \
	fi

fetch_recordings: install-adb
	@echo "Fetching recordings from connected Android device..."
	@bash ./scripts/FileTransfer.sh

configure:
	$(CMAKE) $(CMAKE_FLAGS)

build:
	$(CMAKE_BUILD)

clean:
	@if [ -d $(BUILD_DIR) ]; then rm -rf $(BUILD_DIR); else true; fi

rebuild: clean all
