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

build-integration-tests:
	@$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target integration_tests

build-unit-tests:
	@$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target unit_tests

# Run all tests
test: configure build build-integration-tests build-unit-tests
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Run unit tests only
test-unit: configure build build-unit-tests
	@cd $(BUILD_DIR) && ctest -L unit --output-on-failure

# Run integration tests
test-integration: configure build build-integration-tests
	@cd $(BUILD_DIR) && ctest -L integration --output-on-failure

# Run a specific test by name
test-one: configure build build-integration-tests build-unit-tests
	@cd $(BUILD_DIR) && ctest -R "$(TEST)" --output-on-failure -V

clean:
	@if [ -d $(BUILD_DIR) ]; then rm -rf $(BUILD_DIR); else true; fi

purge-logs:
	rm -rf logs/*

rebuild: clean all purge-logs

.PHONY: all clean rebuild configure build test-plane test-location install-adb fetch_recordings