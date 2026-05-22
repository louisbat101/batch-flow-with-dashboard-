# ESP32-C3 Standalone Batch Flow Controller

## Overview

This is a **complete standalone solution** that combines all functionality into a single ESP32-C3 device. It eliminates the need for RS485 communication and multiple devices by running everything locally.

## Features

✅ **Standalone Operation**: No master/slave architecture - everything runs on one ESP32-C3  
✅ **Dual Hall Flow Sensor**: Single flowmeter with two Hall sensors (A+B) for better accuracy  
✅ **RS485 Valve Control**: Two independent RS485-controlled valves with Modbus RTU  
✅ **Web Interface**: Modern responsive web UI served from LittleFS  
✅ **Product Database**: Store up to 20 products with individual settings on flash storage  
✅ **Manual Batching**: Quick manual batch mode with custom valve/litres  
✅ **Live Status**: Real-time flow readings, valve status, and batch progress  
✅ **Safety Features**: Emergency stop, batch timeouts, valve safety timeouts  
✅ **LED Indicators**: Power LED (always on) + Status LED (activity/batching)  

## Hardware Connections

### ESP32-C3 Pin Assignments (Your PCB Layout)
```
GPIO 2  → Flowmeter Hall Sensor A pulse input
GPIO 7  → Flowmeter Hall Sensor B pulse input  
GPIO 4  → RS485 MAX485 DI (TX)
GPIO 5  → RS485 MAX485 RO (RX)
GPIO 6  → RS485 MAX485 DE+RE (Direction Control)
GPIO 8  → Power LED (always on)
GPIO 10 → Status LED (blinks during activity/batching)
```

### RS485 Valve Wiring
```
ESP32-C3 GPIO 4 → MAX485 DI
ESP32-C3 GPIO 5 ← MAX485 RO  
ESP32-C3 GPIO 6 → MAX485 DE + MAX485 RE
MAX485 A+ ──┐
MAX485 B-  ──┼── RS485 Bus to Valve Controllers
            │
Valve 1 (Modbus Address 1)
Valve 2 (Modbus Address 2)
```

### LED Wiring
```
Power LED:  GPIO 8 → 330Ω resistor → LED → GND
Status LED: GPIO 10 → 330Ω resistor → LED → GND
```

### Flowmeter Wiring
```
Single Flowmeter with Dual Hall Sensors:
Hall Sensor A pulse output → GPIO 2
Hall Sensor B pulse output → GPIO 7
Flowmeter GND → ESP32-C3 GND
Flowmeter VCC → 3.3V or 5V (depending on flowmeter)
```

### MAX485 RS485 Transceiver
```
MAX485 VCC → 3.3V
MAX485 GND → ESP32-C3 GND
MAX485 DI ← GPIO 4 (TX)
MAX485 RO → GPIO 5 (RX)
MAX485 DE ← GPIO 6 (Direction control)
MAX485 RE ← GPIO 6 (Direction control)
MAX485 A+ → RS485+ line to valves
MAX485 B- → RS485- line to valves
```

## Software Build & Flash

### Prerequisites
- [PlatformIO](https://platformio.org/) installed in VS Code or via CLI
- ESP32-C3 development board
- USB cable for programming

### Build Steps

1. **Clone/Copy** this project to your local machine
2. **Open** the `esp32-c3-standalone` folder in VS Code with PlatformIO extension
3. **Build** the firmware:
   ```bash
   platformio run
   ```
4. **Upload** firmware to ESP32-C3:
   ```bash
   platformio run -t upload
   ```
5. **Upload** web files to LittleFS:
   ```bash
   platformio run -t uploadfs
   ```

### Alternative CLI Commands
```bash
cd esp32-c3-standalone
pio run -t upload && pio run -t uploadfs
```

## First-Time Setup

1. **Power** the ESP32-C3
2. **Connect** to WiFi network: `BatchFlow-C3` (password: `batch1234`)
3. **Open browser** to: `http://192.168.4.1/`
4. **Configure products** and calibration settings via the web interface

## Usage

### Web Interface Features

#### 📊 Live Status Panel
- Real-time flow reading (single flowmeter with dual hall sensors)
- Individual Hall A and Hall B pulse counts
- Combined total pulse count
- Current calibration (pulses per litre)
- Valve status (OPEN/CLOSED) for both RS485 valves
- Current batch state and progress
- Emergency stop button

#### 🚀 Quick Batch
- Select valve (1 or 2)
- Enter target litres
- Start manual batch immediately

#### 📦 Products Manager
- Add/edit/delete products
- Set target litres per product
- Configure which RS485 valve to use (1 or 2)
- Adjust single flowmeter calibration per product
- Set valve close timing

### API Endpoints

The device exposes a REST API for integration:

```http
GET  /api/status              # System status and live readings
GET  /api/products            # Product list
POST /api/products            # Add product
PUT  /api/products?id=N       # Update product
DELETE /api/products?id=N     # Delete product
POST /api/batch/start         # Start batch by product ID
POST /api/batch/manual        # Start manual batch
POST /api/batch/stop          # Stop current batch
POST /api/calibrate          # Set flowmeter calibration (single meter)
```

### Example API Usage

**Start manual batch:**
```bash
curl -X POST http://192.168.4.1/api/batch/manual \
  -H "Content-Type: application/json" \
  -d '{"valve": 1, "litres": 2.5}'
```

**Get live status:**
```bash
curl http://192.168.4.1/api/status
```

## Configuration

### Default Settings
- **WiFi AP**: `BatchFlow-C3` / `batch1234`
- **Default Pulses/Litre**: 450 (adjustable per flowmeter)
- **Valve Close Time**: 500ms before target reached
- **Max Batch Size**: 100 litres (safety limit)
- **Max Batch Time**: 5 minutes (safety timeout)
- **Max Products**: 20

### Customization

Edit `src/config.h` to change:
- Pin assignments
- WiFi credentials
- Safety limits
- Default calibration values

## Safety Features

🛑 **Emergency Stop**: Web button stops all batching immediately  
⏱️ **Batch Timeout**: Auto-stop after 5 minutes  
🔒 **Valve Safety**: Auto-close valves after 10 seconds without commands  
📏 **Size Limits**: Maximum 100 litres per batch  
💾 **Data Persistence**: Products and settings saved to flash storage  

## Troubleshooting

### WiFi Connection Issues
- Check device is powered and status LEDs are working
- Scan for `BatchFlow-C3` network
- Password is `batch1234` (case sensitive)
- Try forgetting and reconnecting to WiFi

### Flow Reading Issues
- Check flowmeter Hall sensor A connection to GPIO 2
- Check flowmeter Hall sensor B connection to GPIO 7  
- Verify flowmeter power supply (3.3V or 5V depending on model)
- Calibrate pulses-per-litre in web interface
- Monitor serial output for individual Hall A/B pulse counts
- Ensure both Hall sensors are working (check individual pulse counts)

### RS485 Valve Issues
- Check MAX485 wiring: DI←GPIO4, RO→GPIO5, DE/RE←GPIO6
- Verify RS485 bus connections (A+/B-) to valve controllers
- Check valve controller Modbus addresses (should be 1 and 2)
- Verify valve power supply is adequate
- Test RS485 communication with serial monitor debug output
- Check RS485 bus termination resistors (120Ω at each end)

### Web Interface Issues
- Clear browser cache and reload
- Check console for JavaScript errors
- Try different browser or incognito mode
- Verify ESP32-C3 is not in boot mode

## Serial Monitor

Connect via serial (115200 baud) to see debug output:

```
=== ESP32-C3 Batch Flow Controller ===
[OK] LittleFS mounted
[OK] Database loaded (2 products)
[OK] Dual Hall flow meter initialized
[OK] RS485 valve controller initialized
[OK] LEDs initialized
[OK] WiFi AP started: BatchFlow-C3
[OK] IP address: 192.168.4.1
[OK] Web server started on port 80

=== System Ready ===
Connect to WiFi: BatchFlow-C3 (password: batch1234)
Open browser: http://192.168.4.1/
====================
```

## Differences from Original Project

### What Changed
- ❌ **Removed Multiple Devices**: Everything runs on single ESP32-C3
- ✅ **Updated Flow Sensing**: Single flowmeter with dual Hall sensors (A+B)
- ✅ **Kept RS485 Valves**: Uses existing RS485 valve controllers via Modbus RTU  
- ✅ **Simplified Architecture**: No master/slave communication needed
- ✅ **Improved Web UI**: Modern responsive interface with dual Hall sensor display
- ✅ **Enhanced Safety**: More safety features and timeouts

### What Stayed
- ✅ **RS485 Valve Control**: Same Modbus RTU valve communication protocol
- ✅ **Product Database**: Same JSON-based storage
- ✅ **Calibration System**: Per-flowmeter pulse calibration (now single meter)
- ✅ **Valve Timing**: Configurable close times
- ✅ **API Compatibility**: Similar REST endpoints

## Upgrade Path

This standalone version can be easily **extended** to support multiple lines by:
1. Adding more GPIO pins for additional flowmeters/valves
2. Using GPIO expander ICs for more I/O
3. Adding Ethernet for wired networking
4. Implementing MQTT for remote monitoring

## Support

For issues or questions:
1. Check serial monitor output
2. Verify hardware connections
3. Test with minimal configuration (1 flowmeter + 1 valve)
4. Review this README and configuration files