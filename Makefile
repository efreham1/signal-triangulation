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

configure:
	$(CMAKE) $(CMAKE_FLAGS)

build:
	$(CMAKE_BUILD)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
