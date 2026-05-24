# RS-485 Wiring Diagram - ESP32-C3 Master/Slave

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         BATCH FLOW RS-485 SYSTEM                            │
└─────────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────┐              ┌──────────────────────────────┐
│   MASTER (Custom Build)      │              │   SLAVE (ESC3E05 PCB)        │
│   ESP32-C3 Super Mini        │              │   ESP32-C3 on Expansion PCB  │
├──────────────────────────────┤              ├──────────────────────────────┤
│                              │              │                              │
│  WiFi AP: BatchFlow-Master   │              │  WiFi AP: FlowNode-Setup     │
│  IP: 192.168.4.1             │              │  IP: 192.168.5.1             │
│                              │              │                              │
│  ┌────────────┐              │              │              ┌────────────┐  │
│  │  MAX485    │              │              │              │  MAX485    │  │
│  │ Transceiver│              │              │              │ Transceiver│  │
│  │            │              │              │              │            │  │
│  │ DI ←─GPIO21│              │              │              │GPIO21─→ DI │  │
│  │ RO ─→GPIO 9│              │              │              │GPIO20←─ RO │  │
│  │ DE ←─GPIO20│              │              │              │GPIO 9─→ DE │  │
│  │ RE ←─GPIO20│              │              │              │GPIO 9─→ RE │  │
│  │            │              │              │              │            │  │
│  │  A ────────┼──────────────┼──────────────┼──────────────┼──── A     │  │
│  │            │   Blue Wire  │              │  Blue Wire   │            │  │
│  │  B ────────┼──────────────┼──────────────┼──────────────┼──── B     │  │
│  │            │  Brown Wire  │              │  Brown Wire  │            │  │
│  │ GND ───────┼──────────────┼──────────────┼──────────────┼──── GND   │  │
│  │            │  Black Wire  │              │  Black Wire  │            │  │
│  └────────────┘              │              │              └────────────┘  │
│                              │              │                              │
│  NeoPixel LED: GPIO 4        │              │  NeoPixel LED: GPIO 8        │
│                              │              │  Relay CH1-4: GPIO 5/6/7/10  │
│                              │              │  Flowmeter: GPIO 4 (pulse)   │
└──────────────────────────────┘              └──────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════

PIN ASSIGNMENT SUMMARY:

Master (Custom Wiring):                 Slave (ESC3E05 PCB):
  RS485_RXD_PIN = 9  (your wiring)       RS485_RXD_PIN = 20  ⚠️ NOT 9!
  RS485_TXD_PIN = 21 (standard)          RS485_TXD_PIN = 21  ✅
  RS485_RD_PIN  = 20 (your wiring)       RS485_RD_PIN  = 9   ⚠️ NOT 20!

═══════════════════════════════════════════════════════════════════════════════

WIRE COLORS:
  🔵 Blue  = A (Data+)
  🟤 Brown = B (Data-)
  ⚫ Black = GND (Common Ground)

CRITICAL NOTES:
  1. ESC3E05 PCB silkscreen is WRONG - use GPIO20 for RX!
  2. Both boards MUST share common GND (black wire)
  3. A connects to A, B connects to B (don't cross!)
  4. No termination resistor needed for cables < 3 meters
  5. Keep cables as short as possible to reduce noise

═══════════════════════════════════════════════════════════════════════════════

DIRECTION CONTROL (RD PIN):

  RD = LOW  (0V)  → Receive Mode (RO enabled, DI disabled)
  RD = HIGH (3.3V)→ Transmit Mode (DI enabled, RO disabled)

Master:
  preTransmission():  digitalWrite(RS485_RD_PIN, HIGH);  // TX mode
  postTransmission(): digitalWrite(RS485_RD_PIN, LOW);   // RX mode

Slave:
  setup():     digitalWrite(RS485_RD_PIN, LOW);   // Default: receive
  send():      digitalWrite(RS485_RD_PIN, HIGH);  // Switch to transmit
  after send(): digitalWrite(RS485_RD_PIN, LOW);   // Back to receive

═══════════════════════════════════════════════════════════════════════════════

MODBUS RTU FRAME EXAMPLE:

Request (Master → Slave):
  ┌──┬──┬──────┬──────┬──────┬──────┐
  │01│03│00 00 │00 04 │44 09 │ = 8 bytes
  └──┴──┴──────┴──────┴──────┴──────┘
   │  │    │      │      └─ CRC16 (little-endian)
   │  │    │      └──────── Register count (4)
   │  │    └─────────────── Start register (0x0000)
   │  └──────────────────── Function code (0x03 = Read Holding Regs)
   └─────────────────────── Slave address (1)

Response (Slave → Master):
  ┌──┬──┬──┬────────────────────┬──────┐
  │01│03│08│00 00 00 00 00 00 00│95 D7│ = 13 bytes
  └──┴──┴──┴────────────────────┴──────┘
   │  │  │          │              └─ CRC16 (little-endian)
   │  │  │          └──────────────── 8 bytes data (4 registers)
   │  │  └─────────────────────────── Byte count (8)
   │  └────────────────────────────── Function code (0x03)
   └───────────────────────────────── Slave address (1)

═══════════════════════════════════════════════════════════════════════════════

TIMING DIAGRAM (9600 baud):

  Master TX:  ──┐                    ┌────────────────────
               └────────────────────┘
                │◄─ 8 bytes (~8ms) ─►│

  Delay:                              ─────────────────────
                                      │◄─ 3.5 char (~4ms)─►│

  Slave RX:     ────────────────────┐                    ┌──
                                     └────────────────────┘
                                     │◄─ RD LOW (receive) ─►│

  Slave RD:     ────────────────────┐                    ┌──
                LOW (receive)        │     HIGH (transmit)│
                                     └────────────────────┘

  Slave TX:     ──────────────────────┐                  ┌──
                                       └──────────────────┘
                                       │◄─ 13 bytes (~13ms)─►│

  Master RX:    ──────────────────────┐                  ┌──
                                       └──────────────────┘
                                       │◄─ RD LOW (receive)─►│

═══════════════════════════════════════════════════════════════════════════════

SUCCESS INDICATORS:

Slave Monitor:
  [MB] RX: 8 bytes available              ← Receiving complete frames
  [MB] RX frame: 8 bytes - 01 03...       ← Frame parsed correctly
  [MB] Read regs: status=0 pulses=0...    ← Processing request
  [MB] Sending 13 bytes: 01 03 08...      ← Sending response
  [MB] TX complete                         ← Transmission done

Master Monitor:
  [Modbus] Reading 4 regs from slave 1... ← Sending request
  [Modbus] ✓ SUCCESS! Got 4 registers     ← Received response
  [Poll] Slave 1: ✓ ONLINE, Pulses=0...   ← Communication working

═══════════════════════════════════════════════════════════════════════════════

For setup instructions, see: RS485_QUICK_START.md
For troubleshooting, see: RS485_SETUP_GUIDE.md
For ESC3E05 details, see: ESC3E05_PIN_CORRECTION.md

```
