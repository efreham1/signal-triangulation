# Simple Makefile wrapper that configures and builds with CMake
# Usage:
#   make          # Configure (cmake -S . -B build) and build (cmake --build build)
#   make test     # Configure, build and run ctest
#   make test-unit        # Run unit tests only
#   make test-integration # Run integration tests only
#   make test-one TEST=<test_name>  # Run a specific test by name
#   make clean    # Remove build directory
#   make rebuild  # Clean, configure and build
#   make install-adb
#   make fetch_recordings

BUILD_DIR := build
CMAKE := cmake
NPROC := $(shell nproc 2>/dev/null || echo 1)
CMAKE_FLAGS := -S . -B $(BUILD_DIR)

# Default target
all: release

# --- Build Types ---

# Release: Optimized build (-O3 by default in CMake Release)
release:
	$(CMAKE) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC)

# Debug: No optimization, debug symbols (-g)
debug:
	$(CMAKE) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build $(BUILD_DIR) --config Debug -j$(NPROC)

# Profiling: Optimized + gprof (-O3 -pg)
profiling:
	$(CMAKE) $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Profiling
	$(CMAKE) --build $(BUILD_DIR) --config Profiling -j$(NPROC)

# --- Utilities ---

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

# Tests (default to Release build for performance)
build-integration-tests:
	@$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target integration_tests

build-unit-tests:
	@$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target unit_tests

# Run all tests
test: release build-integration-tests build-unit-tests
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Run unit tests only
test-unit: release build-unit-tests
	@cd $(BUILD_DIR) && ctest -L unit --output-on-failure

# Run integration tests
test-integration: release build-integration-tests
	@cd $(BUILD_DIR) && ctest -L integration --output-on-failure

# Run a specific test by name
test-one: release build-integration-tests build-unit-tests
	@cd $(BUILD_DIR) && ctest -R "$(TEST)" --output-on-failure -V

clean:
	@if [ -d $(BUILD_DIR) ]; then rm -rf $(BUILD_DIR); else true; fi

purge-logs:
	rm -rf logs/*

rebuild: clean release purge-logs

.PHONY: all clean rebuild release debug profiling test test-unit test-integration test-one install-adb fetch_recordings