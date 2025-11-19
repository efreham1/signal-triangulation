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

test-plane: configure
	@$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target plane_fit_tests
	@cd $(BUILD_DIR) && ctest -L plane --output-on-failure

test-location: configure
	@$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target triangulation_tests
	@cd $(BUILD_DIR) && ctest -L location --output-on-failure

clean:
	@if [ -d $(BUILD_DIR) ]; then rm -rf $(BUILD_DIR); else true; fi

purge-logs:
	rm -rf logs/*

rebuild: clean all purge-logs

.PHONY: all clean rebuild configure build test-plane test-location install-adb fetch_recordings