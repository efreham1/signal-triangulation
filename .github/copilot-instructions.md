# Signal Triangulation Project AI Agent Instructions

## Architecture Overview

This C++ project implements a signal triangulation system with three main components:

1. **Server** (`Server.h/cpp`) - Network interface that:
   - Listens for client connections on configured address/port
   - Handles multiple client connections
   - Parses incoming JSON messages into DataPoint structures
   - Passes data to TriangulationService

2. **TriangulationService** (`TriangulationService.h`) - Core algorithm component that:
   - Collects and processes signal strength data points
   - Implements clustering and outlier detection
   - Calculates estimated signal source position
   
3. **DataPoint** (`DataPoint.h`) - Data structure representing signal measurements:
   ```cpp
   struct DataPoint {
       double latitude;
       double longitude;
       int rssi;          // WiFi signal strength
       long long timestamp_ms;
   }
   ```

## Key Implementation Patterns

### Network Communication
- Server uses socket programming (platform-specific headers required)
- Clients connect via TCP to port 12345 by default
- JSON protocol for data transmission (parsing to be implemented)
- Each client handled in separate connection context

### Data Processing Flow
1. Client sends signal strength measurements
2. Server parses JSON into DataPoint objects
3. Points added to TriangulationService for processing
4. Service accumulates points until calculation threshold met
5. Algorithm computes estimated source location

## Development Guidelines

### Build Setup
- Project uses standard C++ compilation (specific build system TBD)
- Platform-specific socket headers needed:
  - Linux/macOS: `<sys/socket.h>`
  - Windows: `<winsock2.h>`

### Code Organization
- Header files (.h) declare interfaces
- Implementation files (.cpp) contain definitions
- Class-per-file structure with clear responsibility separation

### TODOs and Implementation Notes
Major implementation areas marked with TODO comments:
1. Server socket handling and client communication
2. JSON message parsing
3. Core triangulation algorithm in TriangulationService
4. Data point storage and processing strategy

### Error Handling
- Server implements try-catch for high-level error management
- Network errors should be handled at socket operation level
- Invalid data handling needed in message parsing

## Integration Points
- Primary external integration is client-server protocol
- Clients connect via TCP/IP (default localhost:12345)
- JSON message format (schema to be documented)