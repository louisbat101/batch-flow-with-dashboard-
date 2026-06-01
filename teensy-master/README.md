# Teensy 4.1 RS-485 Master

## Status
✅ **Firmware compiled successfully** - Ready to upload

## Upload Instructions

### Option 1: Manual Upload via Teensy Loader (Recommended)
1. Download and install **Teensy Loader** from: https://www.pjrc.com/teensy/loader.html
2. Open Teensy Loader application
3. Press the **Program Button** on your Teensy 4.1 board
4. Teensy Loader will automatically detect and program the device
5. The firmware at `.pio/build/teensy41/firmware.hex` will be loaded

### Option 2: Upload via Arduino IDE
1. Open Arduino IDE
2. Tools > Board > Teensy 4.1
3. Tools > Port > Select Teensy Serial Port
4. Sketch > Upload Using Programmer
5. Select the `.pio/build/teensy41/firmware.hex` file

### Option 3: Command Line (Once Teensy Loader CLI is Installed)
```bash
cd /Users/louishome/working\ projects/batch\ flow/teensy-master
pio run -e teensy41 -t upload
```

## Hardware Configuration

**Teensy 4.1 Pins:**
- Pin 9 (RX2): RS-485 Receive
- Pin 10 (TX2): RS-485 Transmit  
- Pin 11: DE/RE (Direction Control)

**RS-485 Wiring:**
- RS-485 A (Non-inv) → Pin 9 RX
- RS-485 B (Inv) → Pin 10 TX
- DE/RE (Direction) → Pin 11

## Serial Monitor
After upload, view the output:
- Baud Rate: 115200
- Expected output: "Teensy 4.1 running" followed by periodic "tick" messages

## Modbus Configuration
- Baud: 9600
- Protocol: Modbus RTU
- Slaves: Addresses 1-4
- Function Code: FC04 (Read Input Registers)
- Timeout: 300ms

The firmware polls all four boards every 1 second.

## Next Steps
1. Upload firmware using Teensy Loader
2. Verify serial output shows board status
3. Connect RS-485 slave boards (ESP32-C3 at addresses 1-4)
4. Monitor Modbus communication on serial output
