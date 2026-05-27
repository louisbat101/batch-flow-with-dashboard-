# RS-485 Modbus RTU Wiring Guide

## ⚠️ CRITICAL: GPIO 0 is Bootstrap Pin!

**DO NOT use GPIO 0 for RS-485 TX!**

GPIO 0 is the bootstrap pin on ESP32-C3. If GPIO 0 is held LOW during power-up or reset, the chip enters bootloader mode instead of running the application. This causes the board to hang/not boot.

## Current Configuration (CORRECT)

### Pin Mapping
```
Master & Slave ESP32-C3 → ESC3E05 RS-485 Module
┌─────────────────────────────────────────┐
│ GPIO 2  → TXD (UART TX to RS-485)       │
│ GPIO 3  → RXD (UART RX from RS-485)     │
│ GPIO 9  → RD  (Direction Control)       │
│         → GND (Common Ground)           │
└─────────────────────────────────────────┘
```

### Code Configuration
**File: `esp32-c3-master/src/config.h` (Master)**
```cpp
#define RS485_RXD_PIN     3       // GPIO 3 (RX)
#define RS485_TXD_PIN     2       // GPIO 2 (TX)
#define RS485_RD_PIN      9       // Direction control
#define RS485_BAUD        9600    // Modbus RTU standard
```

**File: `esp32-c3-slave/src/slave_config.h` (Slave)**
```cpp
#define RS485_RXD_PIN     3       // GPIO 3 (RX)
#define RS485_TXD_PIN     2       // GPIO 2 (TX)
#define RS485_RD_PIN      9       // Direction control
#define RS485_BAUD        9600    // Modbus RTU standard
```

## Physical Wiring Steps

### Step 1: Bridge GPIO 2 and 3 on PCB
On the **back of both ESP32-C3 boards**, create these bridges:
- **GPIO 2 (TXD)** → solder wire to TXD pad on ESC3E05
- **GPIO 3 (RXD)** → solder wire to RXD pad on ESC3E05
- **GPIO 9 (RD)** → solder wire to RD/DE pad on ESC3E05
- **GND** → connect common ground

### Step 2: RS-485 Termination
At **each end** of the RS-485 bus (both boards):
- Install **120Ω resistor** between A and B lines
- This prevents reflections and signal degradation

### Step 3: Verify
After wiring:
1. **DO NOT connect USB yet** - verify with multimeter first
2. Measure with multimeter between GPIO 2 and GPIO 3
3. Should show no shorts (high resistance)
4. Then connect USB and boot firmware

## Modbus RTU Protocol
- **Baud Rate**: 9600
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Master**: Polls slave every 500ms
- **Timeout**: 50ms per request
- **Slave Address**: 1 (hardcoded)

## Debugging
If RS-485 doesn't work:
1. Check GPIO 2 and 3 voltages with multimeter (should toggle during transmission)
2. Check RS-485 A and B differential (should be ±3V when transmitting)
3. Verify termination resistors are installed
4. Check for loose wires or cold solder joints

## Why GPIO 2/3 Instead of GPIO 0/1?

**GPIO 0**: Bootstrap pin - pulled LOW during boot to enter bootloader mode
- Using GPIO 0 for UART causes chip to hang at startup
- Solution: Use GPIO 2/3 instead (safe, no bootstrap conflict)

**GPIO 1**: USB TX (CDC serial over USB)
- Can conflict with RS-485 if both enabled
- GPIO 3 is safer alternative

## Status
✅ GPIO pins configured safely for RS-485
✅ Bootstrap pin conflict resolved
⏳ Testing with boards
