# RS-485 Pin Configuration Verification

**Status**: ✅ **PIN CONFIGURATION VERIFIED** — All RS-485 pins are correctly configured

---

## Summary Table

| Device | Role | UART | TX Pin | RX Pin | DE/RE Control | Baud Rate | Notes |
|--------|------|------|--------|--------|----------------|-----------|-------|
| **Teensy 4.1** | Master | UART5 (Serial5) | Pin 20 | Pin 21 | GPIO 19 (DE) | 9600 | ✅ Correct |
| **ESP32-C3 Slave** | Slave | UART1 (Serial1) | GPIO 2 | GPIO 3 | GPIO 9 (DE/RE) | 9600 | ✅ Correct |

---

## Device Details

### Teensy 4.1 (Master Controller)

**Firmware Location**: `/teensy-master/src/main.cpp`

```cpp
// ── Teensy 4.1 UART5 RS-485 Configuration ──
// UART5: TX=Pin 20, RX=Pin 21, Direction=Pin 19
#define RS485_TX_PIN 20
#define RS485_RX_PIN 21
#define RS485_DE_PIN 19
#define RS485_BAUD 9600
Serial5.begin(RS485_BAUD);     // UART5 with TX=Pin 20, RX=Pin 21
```

**Pin Mapping**:
- **UART5 TX**: Pin 20 (Serial5 TX) → to RS-485 MAX485 pin DI (Data In)
- **UART5 RX**: Pin 21 (Serial5 RX) → from RS-485 MAX485 pin RO (Receiver Out)
- **GPIO 19**: Direction control → tied to both DE (Driver Enable) and RE (Receiver Enable)
  - `HIGH` = Transmit mode (Teensy driving the bus)
  - `LOW` = Receive mode (Teensy listening)

**Verification**:
- ✅ Serial5 is available and properly configured for RS-485
- ✅ GPIO 19 is available and not used by Serial5
- ✅ CRC16 calculation implemented correctly (0xA001 polynomial)
- ✅ Baud rate matches slave (9600)

---

### ESP32-C3 Slave (Listener Device)

**Firmware Location**: `/esp32-c3-slave/src/slave_config.h`

⚠️ **NOTE**: The header comment in `slave_config.h` incorrectly references "GPIO 9/20/21" but the actual `#define` statements use GPIO 2/3/9. The **actual compiled code uses GPIO 2/3/9** (correct).

```cpp
#define RS485_RXD_PIN     3       // GPIO 3 (RX)  ← ACTUAL PIN
#define RS485_TXD_PIN     2       // GPIO 2 (TX)  ← ACTUAL PIN
#define RS485_RD_PIN      9       // Direction control (DE/RE)  ← ACTUAL PIN
#define RS485_BAUD        9600    // Modbus RTU standard
Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
```

**Pin Mapping**:
- **UART1 TX**: GPIO 2 → to RS-485 MAX485 pin DI (Data In)
- **UART1 RX**: GPIO 3 → from RS-485 MAX485 pin RO (Receiver Out)
- **GPIO 9**: Direction control (DE/RE) → tied to both DE and RE
  - `HIGH` = Transmit mode
  - `LOW` = Receive mode (default)

**Verification**:
- ✅ UART1 is configured with explicit RXD/TXD pins
- ✅ GPIO 9 is available for direction control
- ✅ GPIO 0 avoided (bootstrap pin - dangerous!)
- ✅ Baud rate matches master (9600)

---

## Wiring Checklist

To connect Teensy master to ESP32-C3 slave via RS-485:

### Required Hardware
- [ ] MAX485 RS-485 transceiver module (or equivalent)
- [ ] 2-pair twisted wire (A/B differential lines)
- [ ] Power supply for both boards

### Physical Connections

**From Teensy 4.1 Master to RS-485 Module**:
```
Teensy Pin 20 (TX)  → MAX485 DI  (pin 4)
Teensy Pin 21 (RX)  → MAX485 RO  (pin 1)
Teensy Pin 19 (DE)  → MAX485 DE  (pin 3)
                    → MAX485 RE  (pin 2)  [tie together or use GPIO 19 for both]
Teensy GND          → MAX485 GND (pin 5)
```

**From RS-485 Module to Shared Bus**:
```
MAX485 A (pin 6)    → Shared bus A
MAX485 B (pin 7)    → Shared bus B
```

**From RS-485 Module to ESP32-C3 Slave**:
```
MAX485 A            → C3 (via second RS-485 module DI/RO)
MAX485 B            → C3 (via second RS-485 module DI/RO)
```

**From Second RS-485 Module to ESP32-C3 Slave**:
```
MAX485 DI (pin 4)   ← C3 GPIO 2  (TX)
MAX485 RO (pin 1)   → C3 GPIO 3  (RX)
MAX485 DE (pin 3)   ← C3 GPIO 9  (DE/RE)
MAX485 RE (pin 2)   ← C3 GPIO 9  (DE/RE) [tied to DE]
MAX485 GND (pin 5)  → C3 GND
```

### Communication Flow
```
Teensy (Master)
├─ Queries slave every 1 second
├─ Sends FC04 (Read Input Registers) request
└─ Expects response from address 0x02

↕ RS-485 Bus (9600 baud, 8N1)
↕ MAX485 transceiver (or similar)

ESP32-C3 (Slave)
├─ Listens on address 0x02
├─ Simulates sensor data (flowrate 0-100)
└─ Responds with register values
```

---

## Protocol Details

### Modbus RTU Frame Format
```
[Slave Addr] [Function Code] [Start Addr Hi] [Start Addr Lo] [Qty Regs Hi] [Qty Regs Lo] [CRC Lo] [CRC Hi]
    0x02          0x04           0x00          0x00           0x00         0x02          ?        ?
```

**Teensy Implementation**:
- ✅ Implements full CRC16 with correct polynomial (0xA001)
- ✅ Sets TX mode before transmission, RX mode before waiting
- ✅ 300ms timeout per query
- ✅ Polls addresses 1-4 (currently only 2 has a slave)

**ESP32-C3 Implementation**:
- ✅ Listens on address 2 (configurable via web UI)
- ✅ Responds to FC04 requests
- ✅ Simulates sensor data automatically
- ✅ Direction control defaults to RX mode

---

## Testing Commands

**Verify Teensy is running**:
```bash
timeout 5 cat /dev/cu.usbmodem175441501 2>/dev/null | grep "Polling"
```

**Verify ESP32-C3 is running**:
```bash
timeout 5 cat /dev/cu.usbmodem241201 2>/dev/null | grep "listening"
```

**Expected Output After Wiring**:

Teensy output:
```
[RS485] Board 1: No response
[RS485] Board 2: OK           ← This changes to OK when wired!
[RS485] Board 3: No response
[RS485] Board 4: No response
```

ESP32-C3 output:
```
[Status] Still listening on address 2... (RS485 available: 8 bytes)  ← Now receiving!
```

---

## Common Issues & Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| **All boards show "No response"** | RS-485 not wired | Check physical connections and bus power |
| **Slave shows 0 bytes available** | TX/RX pins reversed | Swap RX/TX connections |
| **CRC errors in logs** | Bad wiring or noise | Use twisted pair, add termination resistors (120Ω) |
| **Only slave responds, master doesn't transmit** | DE pin not working | Check GPIO 11 on Teensy for direction control |
| **Garbled data** | Baud rate mismatch | Verify both set to 9600 |

---

## Pin Safety Notes

✅ **GPIO 11 (Teensy)**: **AVAILABLE** - Now using GPIO 19 for direction control
- Not used by Serial5 ✅

✅ **GPIO 19 (Teensy)**: Safe to use for direction control
- Not used by UART5 or other critical functions
- Properly configured in firmware ✅

---

## Conclusion

**All pin configurations are correct and match the hardware capabilities.**

The system is ready for physical RS-485 wiring. Once the bus is connected, the master will immediately start receiving responses from the slave at address 2.

Next Step: Complete the physical RS-485 connections as outlined above.
