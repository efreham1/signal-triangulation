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

## REST API Server

### Overview
The `rest-api-server` is a lightweight HTTP server for uploading measurement files and running triangulation algorithms remotely. It provides endpoints for file upload, file listing, and algorithm execution, making it easy to integrate with the Polaris Android app or other clients over Wi-Fi.

### Usage

**Start the REST API server:**
```bash
# Default: listen on port 8080, save uploads to "uploads/"
./build/rest-api-server

# Custom port and output directory
./build/rest-api-server --port 9000 --output recordings/

# Show help
./build/rest-api-server --help
```

**Endpoints:**

- `POST /upload`  
  Upload a JSON recording file.  
  Use the `X-Filename` header to specify the filename.

- `GET /files`  
  List all uploaded files in the output directory.

- `GET /run-algorithm?files=file1.json,file2.json,...`  
  Run the triangulation algorithm on the specified files (comma-separated list). Returns the result as JSON.

**Example: Upload a file from the command line**
```bash
curl -X POST http://192.168.1.100:8080/upload \
  -H "X-Filename: recording1.json" \
  -H "Content-Type: application/json" \
  --data-binary @path/to/recording.json
```

**Example: Run the algorithm on uploaded files**
```bash
curl "http://192.168.1.100:8080/run-algorithm?files=recording1.json,recording2.json"
```

**Integration Tips:**
1. Find your computer's local IP: `ifconfig` (macOS/Linux) or `ipconfig` (Windows)
2. Ensure computer and Android device are on the same Wi-Fi network or hotspot
3. Configure firewall to allow incoming connections on the chosen port
4. Use the `X-Filename` header to specify custom filenames for uploads
5. The server creates the output directory automatically if it doesn't exist


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