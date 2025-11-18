# Simple Makefile wrapper that configures and builds with CMake
# Usage:
#   make          # Configure (cmake -S . -B build) and build (cmake --build build)
#   make test     # Configure, build and run ctest
#   make clean    # Remove build directory
#   make rebuild  # Clean, configure and build
#   make install-adb
#   make fetch_recordings

BUILD_DIR := build
CMAKE := cmake
NPROC := $(shell nproc 2>/dev/null || echo 1)
CMAKE_FLAGS := -S . -B $(BUILD_DIR)
CMAKE_BUILD := $(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC)

all: configure build

install-adb:
	@if command -v adb >/dev/null 2>&1; then \
    	echo "adb already installed: $$(command -v adb)"; \
	else \
        if [ "$$(uname -s)" = "Linux" ] || [ "$$(uname -s)" = "Darwin" ]; then \
        	bash ./scripts/install-adb.sh; \
    	else \
        	echo "Unsupported OS for automatic adb install." >&2; \
			exit 1; \
		fi; \
	fi

fetch_recordings:
	@echo "Fetching recordings from connected Android device..."
	@bash ./scripts/FileTransfer.sh

configure:
	$(CMAKE) $(CMAKE_FLAGS)

build:
	$(CMAKE_BUILD)

test: configure build
	@echo "Running ctest in $(BUILD_DIR)..."
	cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

.PHONY: all clean rebuild configure build test install-adb fetch_recordings