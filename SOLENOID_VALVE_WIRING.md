# Solenoid Valve Wiring (VERIFIED WORKING)

## Hardware
- **Solenoid Valve**: 3-wire (Black, Red, White)
- **Relay Module**: ESC3E05 4-Channel (7-25V input)
- **Power Supply**: 12V DC

## Verified Correct Wiring

| Component | Connection | Notes |
|---|---|---|
| **Power Supply +12V** | Relay Module VCC input | Powers the relay coil |
| **Power Supply GND** | Relay Module GND input | Common ground |
| **Relay Module IN1** | GPIO5 (ESP32-C3) | Control signal from firmware |
| **Valve Black (GND)** | Relay Module COM1 | Normally connected to COM |
| **Valve Red (+12V)** | Relay Module COM1 | Gets 12V when relay closes |
| **Valve White (Signal)** | Relay Module NO1 | Normally Open contact |

## Operation
1. Power supply provides +12V to relay module VCC and GND
2. Web UI button "Relay 1 ON" → GPIO5 HIGH → Relay IN1 energizes
3. Relay contact closes: COM1 connects to NO1
4. Circuit completes: 12V (COM1) → NO1 → White wire → Solenoid → Black wire → GND
5. **Valve opens**

Web UI button "Relay 1 OFF" → GPIO5 LOW → Relay contact opens → Valve closes

## Status
✅ **WORKING** - Valve switches correctly with relay control
