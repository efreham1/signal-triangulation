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
	@if command -v adb >/dev/null 2>&1; then \
		echo "adb already installed: $$(command -v adb)"; \
	else \
		if [ "$$(uname -s)" = "Linux" ] || [ "$$(uname -s)" = "Darwin" ]; then \
			bash ./tools/install-adb.sh; \
		else \
			powershell -ExecutionPolicy Bypass -File ./tools/install-adb.ps1; \
		fi; \
	fi

fetch_recordings: install-adb
	@echo "Fetching recordings from connected Android device..."
	@bash ./src/core/FileTransfer.sh

configure:
	$(CMAKE) $(CMAKE_FLAGS)

build:
	$(CMAKE_BUILD)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
