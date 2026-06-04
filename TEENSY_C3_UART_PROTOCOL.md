# Teensy ↔ ESP32-C3 UART Communication Protocol

## Hardware Connection
- **Teensy Pin 1** (RX) ← C3 GPIO 21 (TX)
- **Teensy Pin 2** (TX) → C3 GPIO 20 (RX)
- **Baud Rate**: 115200 (fast for local UART)
- **Format**: 8N1

---

## Protocol Design

### Message Format
```
[START] [CMD] [LEN] [DATA...] [CHECKSUM] [END]
[0xFF]  [1B]  [1B]  [LEN bytes] [1B]       [0xFE]
```

**START**: 0xFF
**END**: 0xFE
**CHECKSUM**: XOR of all data bytes

---

## Command Types

### 1. TEENSY → C3: Status Report (every 1 second)
```
CMD: 0x01
DATA: {
  board_addr: 1B (1-4)
  board_status: 1B (0=offline, 1=idle, 2=dispensing, 3=done)
  target_liters: 4B float
  dispensed_liters: 4B float
  valve_open: 1B (0/1)
  pulse_count: 4B uint32
}
Total: 15 bytes
```

**Example**: Report Board 2 online, target 5L, dispensed 2.5L
```
FF 01 0F 02 01 41A00000 41200000 01 00000064 [checksum] FE
```

### 2. C3 → TEENSY: Control Command
```
CMD: 0x02
DATA: {
  board_addr: 1B (1-4)
  action: 1B (0=stop, 1=start)
  product_id: 1B
  target_liters: 4B float
}
Total: 7 bytes
```

**Example**: Start board 2, product 5, target 10L
```
FF 02 07 02 01 05 41200000 [checksum] FE
```

### 3. C3 → TEENSY: Query Status
```
CMD: 0x03
DATA: {
  board_addr: 1B (0xFF = all boards)
}
Total: 1 byte
```

### 4. TEENSY → C3: Acknowledge
```
CMD: 0x81 (0x01 | 0x80)
DATA: {
  original_cmd: 1B
  success: 1B (0=fail, 1=success)
}
Total: 2 bytes
```

---

## Example Exchange

**C3 sends start command**:
```
→ FF 02 07 02 01 05 41200000 C4 FE
  (Start board 2, product 5, 10L target)
```

**Teensy receives, processes, sends ACK**:
```
← FF 81 02 02 01 3F FE
  (ACK command 2, success=1)
```

**Teensy sends status update**:
```
← FF 01 0F 02 02 41200000 41000000 01 000000FF 7D FE
  (Board 2, dispensing, target 10L, dispensed 8L, valve open, 255 pulses)
```

---

## Implementation Notes

1. **Non-blocking**: All reads use `available()` check + timeout
2. **Buffering**: Max 64-byte receive buffer
3. **Reliability**: CRC-like checksum (XOR)
4. **Polling**: C3 queries Teensy every 500ms if no data received
5. **Timeout**: Boards marked offline if no status for 3 seconds
