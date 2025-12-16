#!/usr/bin/env bash
# Test script for the file-receiver utility
# Starts the receiver in background, uploads a test file, then stops it

set -e

echo "=== File Receiver Test Script ==="

# Check if file-receiver exists
if [[ ! -f "./build/file-receiver" ]]; then
    echo "Error: file-receiver not found. Please run 'make' first."
    exit 1
fi

# Create test file
TEST_FILE=$(mktemp)
cat > "$TEST_FILE" << 'EOF'
{
  "timestamp": "2025-12-10T14:30:00Z",
  "test": "data",
  "measurements": [
    {"ssid": "TestNetwork", "rssi": -45, "lat": 59.3293, "lon": 18.0686}
  ]
}
EOF

echo "Created test file: $TEST_FILE"

# Start receiver in background
echo "Starting file-receiver on port 8888..."
./build/file-receiver --port 8888 --output test_uploads &
RECEIVER_PID=$!

# Wait for server to start
sleep 2

# Check if server is running
if ! kill -0 $RECEIVER_PID 2>/dev/null; then
    echo "Error: file-receiver failed to start"
    rm -f "$TEST_FILE"
    exit 1
fi

echo "Server started (PID: $RECEIVER_PID)"

# Upload test file
echo "Uploading test file..."
if curl -s -X POST http://localhost:8888/ \
    -H "X-Filename: test_recording.json" \
    -H "Content-Type: application/json" \
    --data-binary "@$TEST_FILE"; then
    echo ""
    echo "✓ Upload successful"
else
    echo "✗ Upload failed"
    kill $RECEIVER_PID 2>/dev/null || true
    rm -f "$TEST_FILE"
    exit 1
fi

# Stop server
echo "Stopping server..."
kill $RECEIVER_PID 2>/dev/null || true
wait $RECEIVER_PID 2>/dev/null || true

# Check if file was saved
if [[ -f "test_uploads/test_recording.json" ]]; then
    echo "✓ File saved successfully: test_uploads/test_recording.json"
    echo ""
    echo "File contents:"
    cat test_uploads/test_recording.json
    echo ""
else
    echo "✗ File was not saved"
    rm -f "$TEST_FILE"
    exit 1
fi

# Cleanup
rm -f "$TEST_FILE"
rm -rf test_uploads

echo ""
echo "=== Test Complete ==="
