# Signal Triangulation

## Overview
This repository contains:
- Polaris: an Android data collection app for capturing Wi‑Fi signal measurements alongside GPS fixes.
- A C++ triangulation stack (src/) for parsing incoming measurements, filtering/clusterizing them, and estimating transmitter positions.
- A lightweight file receiver utility for accepting uploaded recordings over HTTP.
- Utility scripts and a Makefile wrapper for building and fetching recordings from devices.

## Repository Structure
```
signal-triangulation/
├── Polaris/                 # Android app (data collector)
├── src/                     # C++ triangulation stack
│   ├── core/
│   │   ├── DataPoint.*                 # Measurement model and helpers
│   │   ├── JsonSignalParser.*          # JSON → DataPoint parsing
│   │   ├── TriangulationService.*      # Orchestrates end‑to‑end pipeline
│   │   ├── ClusteredTriangulationAlgorithm.*  # Clustering + triangulation
│   ├── utils/
│   │   ├── FileReceiver.*              # HTTP file upload receiver
│   └── tools/
│       ├── plane_test.*                # Geometry/solver test harness
├── scripts/               # Helper scripts (ADB install, file transfer, naming)
├── Makefile               # Wrapper for CMake build, ADB install, file transfer
└── build/                 # CMake build output (generated)
    ├── signal-triangulation    # Main triangulation executable
    └── file-receiver           # HTTP file upload receiver
```

## Algorithm (src/)
High‑level pipeline implemented in `src/core` and surfaced via `TriangulationService`:

- Input representation (DataPoint)
  - Encapsulates a single measurement record (signal strength and associated position/metadata).
  - Used throughout parsing, filtering, and solving.

- Parsing (JsonSignalParser)
  - Converts JSON input from clients into `DataPoint` instances.
  - See `JsonSignalParser.h` for the accepted schema and fields.

- Orchestration (TriangulationService)
  - Coordinates parsing, de‑duplication, optional filtering, clustering, and position solving per transmitter/source.
  - Exposes a simple interface to feed measurements and request position estimates.

- Clustering + triangulation (ClusteredTriangulationAlgorithm)
  - Groups consistent measurements (e.g., spatial/temporal coherence) to suppress outliers before solving.
  - Performs multilateration/triangulation in 2D using the clustered set.
  - Typical approach:
    - Convert signal levels to range weights (or relative constraints) via a path‑loss model when calibrated, otherwise weight by signal quality.
    - Solve the position using a least‑squares style estimator with basic robustness to outliers.
  - Returns an estimated position and basic fit/error metrics. See header comments for tunable parameters and outputs.

- Tools (tools/plane_test)
  - Lightweight test harness used to exercise geometry/solver routines with synthetic inputs.

Note: Exact JSON schema, clustering parameters, and solver details are documented in the respective headers (`*.h`) and may evolve.

## Android app: Polaris
- Purpose: collect Wi‑Fi and GPS samples while moving around a transmitter.
- Implements a database with support for exporting recordings to a JSON file.

Required permissions:
- ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION, ACCESS_WIFI_STATE.

Build/Run:
- Android Studio: open `Polaris/` and run on a device.
- CLI: from `Polaris/`, run `./gradlew assembleDebug`.

## Building the C++ stack
Using the top‑level Makefile (Linux/macOS):
```bash
make            # configure + build with CMake
make test       # run ctest (if tests are defined)
make clean      # remove build artifacts
```

Or manually:
```bash
mkdir -p build && cd build
cmake ..
cmake --build . -j"$(nproc 2>/dev/null || echo 1)"
```

This will produce two executables in the `build/` directory:
- `signal-triangulation` - Main triangulation application
- `file-receiver` - HTTP file upload receiver

## File Receiver Utility

### Overview
The `file-receiver` is a lightweight HTTP server for accepting file uploads from Android devices or other clients over Wi-Fi. It replaces the need for Python dependencies and provides a simple, self-contained solution for data transfer.

### Usage

**Start the receiver:**
```bash
# Default: listen on port 8000, save to "uploads/"
./build/file-receiver

# Custom port and output directory
./build/file-receiver --port 9000 --output recordings/

# Show help
./build/file-receiver --help
```

**Upload files from command line:**
```bash
# Upload a JSON recording
curl -X POST http://192.168.1.100:8000/ \
  -H "X-Filename: recording1.json" \
  -H "Content-Type: application/json" \
  --data-binary @path/to/recording.json

# Upload with automatic filename
curl -X POST http://192.168.1.100:8000/ \
  --data-binary @recording.json
```

**Upload from Android (using Polaris or similar):**
```java
// Example HTTP POST with custom filename header
HttpURLConnection conn = (HttpURLConnection) url.openConnection();
conn.setRequestMethod("POST");
conn.setRequestProperty("X-Filename", "recording_" + timestamp + ".json");
conn.setRequestProperty("Content-Type", "application/json");
// ... write data to output stream
```

**Integration Tips:**
1. Find your computer's local IP: `ifconfig` (macOS/Linux) or `ipconfig` (Windows)
2. Ensure computer and Android device are on the same Wi-Fi network
3. Configure firewall to allow incoming connections on chosen port
4. Use the `X-Filename` header to specify custom filenames, otherwise defaults to `upload.bin`
5. The receiver creates the output directory automatically if it doesn't exist

### Testing
Run the included test script to verify the file receiver works:
```bash
./scripts/test_file_receiver.sh
```

This script will:
1. Start the file-receiver on port 8888
2. Upload a test JSON file
3. Verify the file was saved correctly
4. Clean up temporary files


## Fetching recordings from Android
```bash
make fetch_recordings
```
Troubleshooting:
- Ensure scripts run with bash and are executable:
  ```bash
  chmod +x scripts/*.sh
  head -n1 scripts/*.sh   # should be: #!/usr/bin/env bash
  ```
- Verify device visibility:
  ```bash
  adb devices
  ```

## Contributing
- Open an issue or PR with a concise description and reproducible steps.

## License
See LICENSE for details.