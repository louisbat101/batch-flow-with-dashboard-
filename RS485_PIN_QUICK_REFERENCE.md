# RS-485 Pin Quick Reference

## The Actual Pins Being Used (No Confusion!)

### Teensy 4.1 (Master) - UART5
```
TX:    Pin 20  → RS-485 DI (Data In)
RX:    Pin 21  ← RS-485 RO (Receiver Out)
DE/RE: Pin 19  → RS-485 DE+RE (both tied together)
```

### ESP32-C3 Slave  
```
TX:    GPIO 2  → RS-485 DI (Data In)
RX:    GPIO 3  ← RS-485 RO (Receiver Out)
DE/RE: GPIO 9  → RS-485 DE+RE (both tied together)
```

### ESP32-C3 Master (if used)
```
TX:    GPIO 2  → RS-485 DI (Data In)
RX:    GPIO 3  ← RS-485 RO (Receiver Out)
DE/RE: GPIO 9  → RS-485 DE+RE (both tied together)
```

---

## Wiring Diagram (Clean Version)

```
┌──────────────┐              ┌─────────────┐              ┌──────────────┐
│ Teensy 4.1   │              │   MAX485    │              │  ESP32-C3    │
│   Master     │              │ Transceiver │              │    Slave     │
├──────────────┤              ├─────────────┤              ├──────────────┤
│              │              │             │              │              │
│ Pin 20 (TX)  ├─────────────→│ DI (pin 4)  │              │              │
│              │              │             │              │              │
│ Pin 21 (RX)  │←─────────────│ RO (pin 1)  │              │              │
│              │              │             │              │              │
│ Pin 19 (DE)  ├─────┬───────→│ DE (pin 3)  │              │              │
│              │     │        │             │              │              │
│              │     └───────→│ RE (pin 2)  │              │              │
│              │              │             │              │              │
│ GND          ├─────────────→│ GND (pin 5) │              │              │
│              │              │             │              │              │
└──────────────┘              │ A (pin 6)   ├──────────────→│ RS485 Bus A  │
                              │             │    (twisted)  │              │
                              │ B (pin 7)   ├──────────────→│ RS485 Bus B  │
                              │             │     pair      │              │
                              └─────────────┘              │              │
                                                           │ GPIO 2 (TX)  │
                                                           │ GPIO 3 (RX)  │
                                                           │ GPIO 9 (DE)  │
                                                           └──────────────┘
```

---

## All-in-One Connection Checklist

**Between Teensy and MAX485:**
- [ ] Teensy Pin 20 → MAX485 DI
- [ ] Teensy Pin 21 → MAX485 RO
- [ ] Teensy Pin 19 → MAX485 DE
- [ ] Teensy Pin 19 → MAX485 RE (same pin as DE)
- [ ] Teensy GND    → MAX485 GND

**RS-485 Bus (A/B differential lines):**
- [ ] MAX485 A (pin 6) → Twisted Pair (use quality cable!)
- [ ] MAX485 B (pin 7) → Twisted Pair

**Second MAX485 (for ESP32-C3 slave):**
- [ ] RS485 Bus A → MAX485-2 A (pin 6)
- [ ] RS485 Bus B → MAX485-2 B (pin 7)

**Between MAX485-2 and ESP32-C3:**
- [ ] MAX485-2 DI → C3 GPIO 2
- [ ] MAX485-2 RO → C3 GPIO 3
- [ ] MAX485-2 DE → C3 GPIO 9
- [ ] MAX485-2 RE → C3 GPIO 9 (same pin as DE)
- [ ] MAX485-2 GND → C3 GND

---

## Baud Rate (Both Devices)
**9600 baud, 8 data bits, no parity, 1 stop bit (8N1)**

---

## Testing After Wiring

```bash
# Terminal 1: Watch Teensy master
timeout 10 cat /dev/cu.usbmodem175441501 2>/dev/null

# Terminal 2: Watch ESP32-C3 slave
timeout 10 cat /dev/cu.usbmodem241201 2>/dev/null
```

**Expected output from Teensy after wiring:**
```
[RS485] Board 2: OK          ← Changes from "No response" to "OK"
```

**Expected output from ESP32-C3 after wiring:**
```
[Status] Still listening on address 2... (RS485 available: 8 bytes)  ← Now has data!
```

---

## Why These Specific Pins?

- **Teensy**: Uses UART2 (pins 9/10 are hardwired for Serial2)
- **ESP32-C3**: Can use any GPIO, but 2/3 are convenient and safe
- **GPIO 0 is avoided**: Dangerous! It's the bootstrap pin
- **GPIO 11/9 for direction**: Not used by any other peripherals
