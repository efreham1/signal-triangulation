# Simple Makefile wrapper that configures and builds with CMake
# Usage:
#   make          # Configure (if needed) and build
#   make test     # Configure, build and run ctest
#   make clean    # Remove build directory
#   make rebuild  # Clean, configure and build

BUILD_DIR := build
CMAKE := cmake
NPROC := $(shell nproc 2>/dev/null || echo 1)
CMAKE_FLAGS := -S . -B $(BUILD_DIR)
CMAKE_BUILD := $(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC)

# Marker file to track if cmake has been configured
CMAKE_CACHE := $(BUILD_DIR)/CMakeCache.txt

all: build

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

# Only run cmake configure if CMakeCache.txt doesn't exist
$(CMAKE_CACHE): CMakeLists.txt
	$(CMAKE) $(CMAKE_FLAGS)

configure: $(CMAKE_CACHE)

# Build depends on configure being done
build: $(CMAKE_CACHE)
	$(CMAKE_BUILD)

# Specific targets - only build what's needed
signal-triangulation: $(CMAKE_CACHE)
	$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target signal-triangulation

triangulation_tests: $(CMAKE_CACHE)
	$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target triangulation_tests

plane_fit_tests: $(CMAKE_CACHE)
	$(CMAKE) --build $(BUILD_DIR) --config Release -j$(NPROC) --target plane_fit_tests

test-plane: plane_fit_tests
	@cd $(BUILD_DIR) && ctest -L plane --output-on-failure

test-location: signal-triangulation triangulation_tests
	@echo ""
	@./$(BUILD_DIR)/tests/triangulation_tests --gtest_filter=Triangulation.GlobalSummary

# Force reconfigure
reconfigure:
	$(CMAKE) $(CMAKE_FLAGS)

clean:
	@if [ -d $(BUILD_DIR) ]; then rm -rf $(BUILD_DIR); else true; fi

purge-logs:
	rm -rf logs/*

rebuild: clean all purge-logs

.PHONY: all clean rebuild configure build test-plane test-location install-adb fetch_recordings reconfigure signal-triangulation triangulation_tests plane_fit_tests