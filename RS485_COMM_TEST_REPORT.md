# RS-485 Communication Test Results

**Date**: June 2, 2026
**Status**: ⚠️ **Data Received but Corrupted** - Likely A/B lines swapped

---

## Test Data

### Teensy 4.1 Master - Sending ✅
```
[TX] Sending to Board 2: 02 04 00 00 00 02 71 F8
```
**Expected Modbus RTU Request**:
- `02` = Slave Address 2
- `04` = Function Code 4 (Read Input Registers)
- `00 00` = Starting register 0x0000
- `00 02` = Read 2 registers
- `71 F8` = CRC16 checksum

✅ **Teensy is transmitting correctly!**

---

### ESP32-C3 Slave - Receiving ❌
```
[RS485] Received 7 bytes: BF DF FF FF FB 1D 0F
```

❌ **Data is corrupted**
- Expected 8 bytes, got 7
- Expected: `02 04 00 00 00 02 71 F8`
- Received: `BF DF FF FF FB 1D 0F`

---

## Root Cause Analysis

### Pattern Analysis

The received bytes `BF DF FF FF FB 1D 0F` appear to be **bit-inverted** or **polarity-reversed** versions of the transmitted data.

- Received byte 1: `BF` (binary: 10111111)
- If we flip bits: `40` (binary: 01000000) - **NOT a match to `02`**

This suggests: **RS-485 differential pair (A/B lines) are swapped**

---

## Physical Wiring Issue

RS-485 uses differential signaling:
- **Line A** and **Line B** carry opposite signals
- If A and B are **swapped**, the receiver sees **inverted/corrupted data**

### Current Wiring (Faulty):
```
Teensy → MAX485 → ESP32-C3
  ✅ TX works (slave is receiving)
  ❌ RX doesn't work (master gets no response)
  ❌ Data corrupted (A/B lines reversed)
```

---

## Solution

### Fix: Swap the A/B twisted pair at the slave side

**Option 1** (Easiest - swap at slave):
```
MAX485-2 A (pin 6) → Connect to what was connected to B
MAX485-2 B (pin 7) → Connect to what was connected to A
```

**Option 2** (Swap at master):
```
MAX485-1 A (pin 6) → Connect to what was connected to B
MAX485-1 B (pin 7) → Connect to what was connected to A
```

**Option 3** (Swap the cable):
- Physically reverse the twisted pair at either end

---

## Expected Results After Fix

**Teensy Master Output** (should change from "No response"):
```
[TX] Sending to Board 2: 02 04 00 00 00 02 71 F8
[RS485] Board 2: OK        ← Changes from "No response"!
```

**ESP32-C3 Slave Output** (should validate correctly):
```
[RS485] Received 8 bytes: 02 04 00 00 00 02 71 F8
[Modbus] Valid FC04 request received
[RS485] Sending response...
```

---

## Verification Checklist

After swapping A/B lines:

1. **Watch Teensy output**:
   ```bash
   timeout 10 cat /dev/cu.usbmodem175441501 2>/dev/null | grep -E "TX|Board 2"
   ```
   - Should see: `[RS485] Board 2: OK`

2. **Watch ESP32-C3 output**:
   ```bash
   timeout 10 cat /dev/cu.usbmodem241201 2>/dev/null | head -20
   ```
   - Should see valid 8-byte frames without "[Modbus] Invalid" errors

---

## Technical Details

### Why RS-485 A/B polarity matters:

RS-485 is a differential bus. The receiver measures the voltage difference between A and B:
- Normal: A = HIGH, B = LOW → Receiver sees logic 1
- Reversed: B = HIGH, A = LOW → Receiver sees inverted logic

When A/B are swapped:
- All bits are inverted
- All bytes become corrupted
- CRC fails
- Modbus parser rejects the frame

---

## Next Steps

1. **Locate the RS-485 wiring** (twisted pair between MAX485 modules)
2. **Swap A and B at one end** (easiest at the slave side)
3. **Reconnect and test** using the verification commands above
4. **Report results** - should show Board 2: OK
