#!/bin/bash

# ESP32-C3 Master Firmware Auto-Detection and Upload Script
# Detects ESP32-C3 on USB and uploads master firmware

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MASTER_DIR="$SCRIPT_DIR/esp32-c3-master"

echo "🔍 Scanning for ESP32-C3 on USB..."
echo ""

# Detect ESP32-C3 serial ports
C3_PORTS=()

# Check for common ESP32-C3 identifiers
for port in /dev/cu.usbmodem* /dev/ttyUSB* /dev/ttyACM*; do
    if [ -e "$port" ]; then
        # Try to get device info
        if grep -q "241201\|esp32" <(ls -la "$port" 2>/dev/null || echo ""); then
            C3_PORTS+=("$port")
            echo "✓ Found ESP32-C3: $port"
        fi
    fi
done

# Alternative: Check using pio device list
echo ""
echo "Checking with PlatformIO device list..."
PIO_DEVICES=$(pio device list 2>/dev/null || echo "")
echo "$PIO_DEVICES"

echo ""

# Find the most likely C3 port
if [ ${#C3_PORTS[@]} -eq 0 ]; then
    echo "❌ No ESP32-C3 detected on USB"
    echo ""
    echo "Troubleshooting:"
    echo "1. Check USB cable is connected"
    echo "2. Try: ls -la /dev/cu.usbmodem*"
    echo "3. Try: pio device list"
    exit 1
fi

# Use the first detected port
PORT="${C3_PORTS[0]}"
echo "✓ Using port: $PORT"
echo ""

# Check if it's actually a C3 by querying chip info
echo "🔎 Verifying chip type..."
CHIP_INFO=$(esptool.py --port "$PORT" chip_id 2>/dev/null || echo "")
if echo "$CHIP_INFO" | grep -q "ESP32-C3"; then
    echo "✓ Confirmed: ESP32-C3"
else
    echo "⚠ Could not verify chip type, proceeding anyway..."
fi

echo ""
echo "📦 Building master firmware..."
cd "$MASTER_DIR"

# Clean and build
rm -rf .pio/build
pio run -e esp32c3-master -q

echo "✓ Build complete"
echo ""
echo "⬆️  Uploading to $PORT..."
pio run -e esp32c3-master -t upload --upload-port "$PORT" -q

echo "✓ Firmware upload complete!"
echo ""
echo "📁 Uploading LittleFS files..."
pio run -e esp32c3-master -t uploadfs --upload-port "$PORT" -q

echo "✓ LittleFS upload complete!"
echo ""
echo "✅ Master board ready!"
echo ""
echo "📱 Connect to WiFi: BatchFlow-Master"
echo "🔐 Password: batchflow123"
echo "🌐 Access: http://192.168.4.1/"
echo ""
