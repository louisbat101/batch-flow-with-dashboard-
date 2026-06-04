# RS-485 Connection Test Guide

## Current Status
- ✅ **Teensy 4.1 Master**: Running, polling addresses 1-4
- ✅ **ESP32-C3 Slave**: Running, listening on address 2
- ⏳ **Communication**: Not detected (RS-485 bus not wired yet)

## RS-485 Wiring Checklist

### Required Connections
You need an RS-485 transceiver board (like MAX485 or similar):

```
Teensy 4.1          RS-485 Module       ESP32-C3 Slave
═════════════       ═════════════       ══════════════
Pin 10 (TX)    →    DI (Data In)
Pin 9 (RX)     ←    RO (Receiver Out)
Pin 11 (DE)    →    DE (Driver Enable)
GND            →    GND
               
               →    A (Non-inverting)  → A (Non-inv)
               →    B (Inverting)      → B (Inverting)
               →    GND                → GND
```

### Optional but Recommended
- **Termination Resistors**: 120Ω between A and B at each end
- **Shielded Cable**: Use twisted pair with shield connected to GND
- **Decoupling Caps**: 0.1µF on RS-485 module power supply

## Test Sequence

### 1. Verify Wiring
- [ ] Teensy pin 10 → RS-485 DI
- [ ] Teensy pin 9 → RS-485 RO
- [ ] Teensy pin 11 → RS-485 DE
- [ ] RS-485 A → ESP32-C3 Slave A
- [ ] RS-485 B → ESP32-C3 Slave B
- [ ] All GNDs connected together

### 2. Monitor Teensy Output
```bash
timeout 15 cat /dev/cu.usbmodem175441501 2>/dev/null | head -50
```

Expected when connected:
```
[RS485] Board 2: OK
```

### 3. Monitor ESP32-C3 Slave Output
```bash
timeout 15 cat /dev/cu.usbmodem241201 2>/dev/null | head -50
```

Expected when receiving queries:
```
[Status] Still listening on address 2... (RS485 available: 8 bytes)
```

## Troubleshooting

| Symptom | Cause | Solution |
|---------|-------|----------|
| "No response" on all boards | RS-485 not wired | Check wiring connections |
| Garbage data in serial | Baud rate mismatch | Verify 9600 baud on all devices |
| One-way communication | TX/RX reversed | Swap pins 9 and 10 on Teensy |
| Intermittent detection | Bad termination | Add 120Ω resistors between A/B |
| Noise in signal | Missing shielding | Use twisted pair shielded cable |

## Expected System Behavior

Once wired correctly:

**Teensy 4.1 (Master):**
```
[RS485] Board 2: OK
```

**ESP32-C3 Slave:**
```
[Status] Still listening on address 2... (RS485 available: 8 bytes)
```

Both will show successful communication!

---
**Next Steps:**
1. Wire the RS-485 connections
2. Monitor both serial outputs
3. Look for "[RS485] Board 2: OK" on Teensy
4. Board 2 status should change to ONLINE in ESP32-C3 master dashboard
