# Project Polaris

A signal source localization system using cluster-based Angle-of-Arrival (AoA) triangulation algorithms. The project consists of a C++ triangulation engine, a REST API server, and an Android companion app for data collection.

## Overview

Project Polaris estimates the location of a signal source (e.g., a radio transmitter) from a set of GPS-tagged signal strength measurements. The core algorithm partitions measurements into spatial clusters, estimates the angle of arrival (AoA) for each cluster using gradient-based methods, and then optimizes a global cost function to find the most likely source location.

## Features

- **Cluster-based triangulation algorithms** (CTA1 and CTA2)
- **REST API server** for remote signal processing
- **Android app (Polaris)** for collecting GPS-tagged signal measurements
- **Real-time visualization** of results via Python plotting scripts
- **Comprehensive test suite** with unit and integration tests

## Project Structure

```
├── src/                    # C++ source code
│   ├── core/               # Core algorithm implementation
│   │   ├── ClusteredTriangulationAlgorithm1.cpp/h
│   │   ├── ClusteredTriangulationAlgorithm2.cpp/h
│   │   ├── Cluster.cpp/h
│   │   ├── DataPoint.cpp/h
│   │   └── JsonSignalParser.cpp/h
│   ├── rest/               # REST API server
│   ├── main.cpp            # CLI application entry point
│   └── main_rest_api.cpp   # REST server entry point
├── Polaris/                # Android companion app (Kotlin)
├── plotting/               # Python visualization scripts
├── tests/                  # Unit and integration tests
├── Recordings/             # Sample signal recording files (JSON)
└── scripts/                # Utility scripts
```

## Requirements

### Build Dependencies

- CMake 3.14+
- C++17 compatible compiler (GCC, Clang, MSVC)
- OpenMP (optional, for parallel processing)

The following dependencies are automatically fetched via CMake's FetchContent:
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.2
- [spdlog](https://github.com/gabime/spdlog) v1.14.1
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) v0.14.3
- [GoogleTest](https://github.com/google/googletest) (for testing)

### Android App

- Android Studio
- Gradle
- Android SDK

### Plotting (Optional)

- Python 3.x
- matplotlib
- numpy
- pandas

## Building

### Quick Start

```bash
# Build release version
make

# Build debug version
make debug

# Build with profiling support
make profiling

# Clean build directory
make clean

# Full rebuild
make rebuild
```

### Manual CMake Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

## Usage

### Command Line Interface

```bash
./build/signal-triangulation [options] <signals_file.json>
```

#### Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `--param-help` | Show algorithm parameter help |
| `-a, --algorithm <name>` | Algorithm to use: `CTA1` or `CTA2` (default: CTA2) |
| `-p, --plot` | Enable plotting output |
| `--log-level <level>` | Set log level (trace, debug, info, warn, error) |

### Example

```bash
# Run triangulation on a recording file
./build/signal-triangulation Recordings/HalfMoon1.json

# Run with plotting enabled
./build/signal-triangulation -p Recordings/FootballField2.json

# Use CTA1 algorithm
./build/signal-triangulation -a CTA1 Recordings/MergedUrban.json
```

### With Visualization

Pipe the output to the plotting script for visualization:

```bash
./build/signal-triangulation -p Recordings/HalfMoon1.json | python3 plotting/plot_from_stdin.py
```

### REST API Server

```bash
./build/rest-api-server
```

The server accepts signal data via HTTP and returns triangulation results.

## Testing

```bash
# Run all tests
make test

# Run unit tests only
make test-unit

# Run integration tests only
make test-integration

# Run a specific test
make test-one TEST=test_triangulation
```

## Android App (Polaris)

The Polaris Android app collects GPS-tagged signal strength measurements that can be processed by the triangulation engine.

### Building the App

```bash
cd Polaris
./gradlew assembleDebug
```

### Fetching Recordings from Device (optional)

```bash
# Install ADB if needed
make install-adb

# Transfer recordings from connected Android device
make fetch_recordings
```

## Algorithm Overview

The system implements cluster-based Angle-of-Arrival (AoA) triangulation:

1. **Clustering**: Partition GPS-tagged signal measurements into spatial clusters
2. **AoA Estimation**: Fit a local plane to each cluster's signal strength field; the gradient indicates the direction toward the source
3. **Vector Creation**: Convert each cluster into a weighted direction vector
4. **Optimization**: Minimize a cost function based on perpendicular distances from candidate locations to cluster rays
5. **Outlier Rejection**: Identify and exclude anomalous clusters for robustness

## Input Data Format

Signal data is provided in JSON format. The following is an example of a single measurement point:

```json
{
  "measurements": [
    {
      "deviceID": "cb32e7f6ba0ea81a", //unique identifier for the recording device
      "id": 101,
      "latitude": 59.86614995555554,
      "longitude": 17.70517585555555,
      "rssi": -73,
      "ssid": "wifi-hotspot",
      "timestamp": 1764845637110 //timestamp in UNIX time format
    }
  ]
}
```

Sample recordings are provided in the `Recordings/` and `oldRecordings/` directories.