# RS-485 Modbus RTU Setup Guide - CRITICAL NOTES

## 🚨 IMPORTANT PIN CONFIGURATION DIFFERENCES 🚨

### ESC3E05 PCB (Commercial Slave Board) - **DOCUMENTATION IS WRONG!**

**Official Documentation Says:**
- RD = GPIO 9
- RXD = GPIO 20
- TXD = GPIO 21

**❌ THIS IS INCORRECT! ❌**

**ACTUAL WORKING CONFIGURATION (Discovered through testing):**
```cpp
#define RS485_RXD_PIN     20      // RX - Receive Data (NOT GPIO 9!)
#define RS485_TXD_PIN     21      // TX - Transmit Data (CORRECT)
#define RS485_RD_PIN      9       // Direction Control (NOT GPIO 20!)
#define RS485_BAUD        9600    // Standard Modbus RTU baud rate
```

**WHY THIS MATTERS:**
- The PCB silkscreen/documentation has GPIO9 and GPIO20 labels **SWAPPED**
- GPIO20 is physically connected to the RS-485 transceiver RX pin
- GPIO9 is physically connected to the direction control (DE/RE pins)

---

### Custom Master Board (Your Build)

**Pin Configuration:**
```cpp
#define RS485_RXD_PIN     9       // RX - Receive Data (your wiring)
#define RS485_TXD_PIN     21      // TX - Transmit Data
#define RS485_RD_PIN      20      // Direction Control (your wiring)
#define RS485_BAUD        9600    // Standard Modbus RTU baud rate
```

**These pins match YOUR physical wiring when you built the board.**

---

## 📡 RS-485 Physical Wiring

### Wire Colors (Standard)
- **Blue** = A (Data+)
- **Brown** = B (Data-)
- **Black** = GND (Common Ground)

### Connection Diagram
```
Master Board          RS-485 Line          Slave Board (ESC3E05)
  GPIO 21 TX ----→ MAX485 DI                    
  GPIO 9  RX ←---- MAX485 RO         ┌─────→ GPIO 20 (actual RX!)
  GPIO 20 RD ----→ MAX485 DE/RE      │
                   MAX485 A  ←-------┼-------→ Blue Wire (A)
                   MAX485 B  ←-------┼-------→ Brown Wire (B)
                   GND       ←-------┴-------→ Black Wire (GND)
```

### Critical Notes:
1. **All boards must share common GND** - Black wire connects all GNDs
2. **A to A, B to B** - Don't cross the data lines
3. **No termination needed** for short cable runs (<3 meters)
4. **Shorter cables = better** - Reduces signal noise

---

## ⚙️ Direction Control Pin (RD) Usage

The RD pin controls the RS-485 transceiver mode:

### Master (Transmitter Mode Most of Time)
```cpp
// Before transmitting
digitalWrite(RS485_RD_PIN, HIGH);  // Enable transmit mode
Serial1.write(data, len);
Serial1.flush();
digitalWrite(RS485_RD_PIN, LOW);   // Back to receive mode
```

### Slave (Receiver Mode Most of Time)
```cpp
// Initialize in receive mode
pinMode(RS485_RD_PIN, OUTPUT);
digitalWrite(RS485_RD_PIN, LOW);  // LOW = Receive enabled

// When sending response
digitalWrite(RS485_RD_PIN, HIGH);  // Switch to transmit
Serial1.write(response, len);
Serial1.flush();
digitalWrite(RS485_RD_PIN, LOW);   // Back to receive
```

---

## 🔍 How We Discovered the Pin Issue

### Testing Process:
1. **Initial Failure**: Using documented pins (GPIO9=RX) resulted in ZERO bytes received
2. **Raw Byte Test**: Created simple firmware to print ANY received bytes
3. **Pin Swap Test**: Swapped GPIO9 and GPIO20 assignments
4. **Success**: GPIO20=RX immediately started receiving data!

### Test Code Used:
```cpp
// Simple test to verify RX pin
void loop() {
  if (Serial1.available()) {
    uint8_t byte = Serial1.read();
    Serial.printf("[RX] Byte: 0x%02X\n", byte);
  }
}
```

**Result**: No output with GPIO9, immediate output with GPIO20 ✅

---

## 📋 Modbus RTU Protocol Details

### CRC Byte Order - **CRITICAL!**
Modbus uses **LITTLE-ENDIAN** byte order for CRC:

**WRONG (Big-Endian):**
```cpp
uint16_t crcRx = (frame[len-2] << 8) | frame[len-1];  // ❌
```

**CORRECT (Little-Endian):**
```cpp
uint16_t crcRx = frame[len-1] << 8 | frame[len-2];  // ✅
// Low byte first, high byte second
```

### Frame Timing
At 9600 baud, bytes arrive ~1ms apart. Must buffer complete frame:

```cpp
void update() {
  if (!Serial1.available()) return;
  
  delay(5);  // Wait for complete frame (critical!)
  
  // Now read all available bytes
  while (Serial1.available()) {
    frame[frameLen++] = Serial1.read();
  }
}
```

**Without the 5ms delay**: Frames arrive fragmented (1-2 bytes at a time)  
**With the 5ms delay**: Complete 8-byte frames received ✅

---

## 🛠️ Troubleshooting Guide

### Symptom: Slave receives ZERO bytes
**Cause**: Wrong RX pin assignment  
**Solution**: For ESC3E05, use GPIO20 (not GPIO9)

### Symptom: Frames are fragmented (1-3 bytes)
**Cause**: Reading too fast, before full frame arrives  
**Solution**: Add `delay(5)` after detecting first byte

### Symptom: CRC errors on every frame
**Cause**: Byte order wrong (big-endian instead of little-endian)  
**Solution**: Swap CRC byte order: `frame[len-1] << 8 | frame[len-2]`

### Symptom: Slave receives but doesn't respond
**Cause**: RD pin not toggling for transmit  
**Solution**: Set RD pin HIGH before transmit, LOW after

### Symptom: Master sees echoed bytes
**Cause**: RD pin timing issue or half-duplex collision  
**Solution**: Use ModbusMaster library - handles echo filtering automatically

---

## 📚 Library Recommendations

### For Master (Transmitter):
**Use ModbusMaster library** (4-20ma/ModbusMaster)

**Why?**
- Handles TX echo filtering automatically
- Proper timing and delays built-in
- CRC calculation verified
- Detailed error codes (0xE2 = timeout, etc.)
- Production-tested and reliable

**Installation:**
```ini
lib_deps = 
    https://github.com/4-20ma/ModbusMaster.git
```

**Usage:**
```cpp
#include <ModbusMaster.h>

ModbusMaster node;

void preTransmission() { digitalWrite(RS485_RD_PIN, HIGH); }
void postTransmission() { digitalWrite(RS485_RD_PIN, LOW); }

void setup() {
  node.begin(1, Serial1);  // Slave ID, Serial port
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}

void loop() {
  uint8_t result = node.readHoldingRegisters(0, 4);
  if (result == node.ku8MBSuccess) {
    // Success!
  }
}
```

---

## ✅ Success Indicators

### Slave Monitor Should Show:
```
[MB] RX: 8 bytes available
[MB] RX frame: 8 bytes - 01 03 00 00 00 04 44 09 
[MB] Read regs: status=0 pulses=0 valve=0
[MB] Sending 13 bytes: 01 03 08 00 00 00 00 00 00 00 00 95 D7 
[MB] TX complete
```

### Master Monitor Should Show:
```
[Modbus] Reading 4 regs from slave 1 starting at reg 0...
[Modbus] ✓ SUCCESS! Got 4 registers
  Reg[0] = 0x0000 (0)
  Reg[1] = 0x0000 (0)
  Reg[2] = 0x0000 (0)
  Reg[3] = 0x0000 (0)
[Poll] Slave 1: ✓ ONLINE, Pulses=0, Valve=CLOSED
```

---

## 🎯 Quick Reference Summary

| Component | RX Pin | TX Pin | RD Pin | Baud |
|-----------|--------|--------|--------|------|
| **ESC3E05 Slave** | GPIO 20 | GPIO 21 | GPIO 9 | 9600 |
| **Custom Master** | GPIO 9 | GPIO 21 | GPIO 20 | 9600 |

**Wire Colors:**
- Blue = A (Data+)
- Brown = B (Data-)
- Black = GND

**Key Points:**
1. ESC3E05 documentation is WRONG - actual RX is GPIO20
2. Use 5ms delay to buffer complete Modbus frames
3. CRC is little-endian (low byte first)
4. Toggle RD pin: HIGH=transmit, LOW=receive
5. Use ModbusMaster library on master for reliability

---

## 📝 Date Discovered
May 23, 2026

## 🔧 Tools Used
- PlatformIO
- ESP32-C3 Super Mini
- ESC3E05 Expansion Board
- MAX485 RS-485 Transceiver
- Logic analyzer (byte inspection via Serial monitor)

---

## ⚠️ DO NOT FORGET!
**If using ESC3E05 PCB:**
```cpp
#define RS485_RXD_PIN     20  // NOT 9!
#define RS485_RD_PIN      9   // NOT 20!
```

**Ignore the PCB silkscreen - it's wrong! Use these pin assignments!** ✅
