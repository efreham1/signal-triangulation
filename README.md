# Signal Triangulation Project

## Overview
This project implements a signal triangulation system that processes WiFi signal strength data from multiple client devices to estimate the location of a signal source. It uses a client-server architecture with real-time data processing capabilities.

## Features
- TCP/IP server handling multiple client connections
- JSON-based communication protocol
- Real-time signal strength data processing
- Configurable triangulation algorithms
- Cross-platform support (Windows, Linux, macOS)

## Project Structure
```
signal-triangulation/
├── src/
│   ├── core/           # Core functionality
│   │   ├── DataPoint.h
│   │   └── TriangulationService.h
│   ├── network/        # Network-related components
│   │   ├── Server.h
│   │   └── Server.cpp
│   └── main.cpp        # Application entry point
├── include/           # Public headers
├── tests/            # Test files
└── docs/             # Documentation
```

## Prerequisites
- C++ compiler with C++11 support
- Platform-specific socket libraries:
  - Linux/macOS: sys/socket.h
  - Windows: winsock2.h
- JSON parsing library (to be determined)

## Building the Project

### Linux/macOS
```bash
mkdir build && cd build
cmake ..
make
```

### Windows
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build .
```

## Running the Server
```bash
./signal-triangulation [options]
```

Default configuration:
- Host: 127.0.0.1
- Port: 12345

## Contributing
1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## License
[License Type] - See LICENSE file for details