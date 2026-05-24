# ESC3E05 RS-485 Pin Configuration - CORRECTED

## ⚠️ OFFICIAL DOCUMENTATION IS WRONG ⚠️

### What the PCB Silkscreen Says:
```
RD  → GPIO 9
RXD → GPIO 20  
TXD → GPIO 21
```

### ❌ THIS IS INCORRECT! ❌

### Actual Working Configuration (Verified May 23, 2026):
```cpp
// CORRECT pins for ESC3E05 expansion board:
#define RS485_RXD_PIN     20      // ✅ Receive Data
#define RS485_TXD_PIN     21      // ✅ Transmit Data  
#define RS485_RD_PIN      9       // ✅ Direction Control (DE/RE)
#define RS485_BAUD        9600
```

## How We Discovered This:

1. **Initial Setup**: Used pins per documentation (GPIO9=RX, GPIO20=RD)
2. **Result**: ZERO bytes received on GPIO9
3. **Test**: Created raw byte reader to test each pin
4. **Discovery**: GPIO20 receives data, GPIO9 does not!
5. **Conclusion**: PCB silkscreen has GPIO9 and GPIO20 **SWAPPED**

## Proof:
```cpp
// Test code that revealed the truth:
Serial1.begin(9600, SERIAL_8N1, 9, 21);   // GPIO9 RX - NO DATA ❌
Serial1.begin(9600, SERIAL_8N1, 20, 21);  // GPIO20 RX - WORKS! ✅
```

## Wire Colors (Standard):
- **Blue** = A (Data+)
- **Brown** = B (Data-)  
- **Black** = GND

## Usage Example:
```cpp
#include "slave_config.h"

void setup() {
  Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
  pinMode(RS485_RD_PIN, OUTPUT);
  digitalWrite(RS485_RD_PIN, LOW);  // Receive mode
}

void sendResponse(uint8_t* data, int len) {
  digitalWrite(RS485_RD_PIN, HIGH);  // Transmit mode
  Serial1.write(data, len);
  Serial1.flush();
  digitalWrite(RS485_RD_PIN, LOW);   // Back to receive mode
}
```

## Remember:
**ALWAYS use GPIO20 for RX on ESC3E05 board, regardless of what the silkscreen says!**

---
*Last Updated: May 23, 2026*  
*Board: ESC3E05 4-Channel Relay Expansion Board with ESP32-C3*
