# Quick RS-485 Wiring Test

**Problem:** Slave not receiving Modbus frames, even with working firmware from yesterday.

## Test 1: Verify RS-485 Wiring

1. **Check physical connections:**
   - Master A terminal → Slave A terminal (should be same color wire)
   - Master B terminal → Slave B terminal (should be same color wire)
   - Ground connection between boards

2. **Look for loose wires** - if a wire came unplugged overnight, that explains everything

3. **Check power** - make sure both boards have USB power and the RS-485 transceivers are powered

## Test 2: Loopback Test

If wiring looks good, try connecting Master A to Master B (loopback) to test if master's RS-485 is working.

## Expected vs Actual

**Yesterday (Working):**
- Master transmitting ✅
- Slave receiving ✅  
- Modbus communication ✅

**Today (Not Working):**
- Master transmitting ✅ (we see timeout errors = it's trying)
- Slave receiving ❌ (no `[MB] RX:` messages at all)
- Slave not even booting/printing to Serial ❌

## Most Likely Cause

**Physical disconnection** - an RS-485 wire (A or B) came loose/unplugged overnight.

Check the screw terminals or connectors on both boards!
