# 🔍 Troubleshooting: Slave Not Responding

**Date:** May 24, 2026  
**Status:** Slave board not receiving Modbus frames from master

## 🚨 Problem

- ✅ Master is transmitting (error 0xE2 timeout - means it's trying)
- ❌ Slave is NOT receiving anything (no `[MB] RX:` messages)
- ⚠️  Slave serial shows only LED blink messages, no boot info

## 📊 Diagnosis

### What We See:
**Master Output:**
```
[Modbus] Reading 4 regs from slave 1 starting at reg 0...
[Modbus] ✗ FAILED! Error code: 0xE2
  Error: Response Timed Out
```

**Slave Output:**
```
[LED] Color set to 0x00FFFF
[LED] Color set to 0x00FFFF
[LED] Color set to 0x00FFFF
```

### What's Missing:
We should see these boot messages on slave:
```
"ESP32-C3 Modbus Slave + FlowMeter"
"✓ Slave Address: 1"
"[MB] Modbus slave 1 on RS485 (GPIO20 RX, GPIO21 TX, GPIO9 RD=LOW)"
"✓ Slave ready! Waiting for Modbus commands..."
```

##  Possible Causes

1. **Slave crashed during boot** - maybe WiFi or web server init failing
2. **Serial1 not initialized** - Modbus UART not set up
3. **Preferences (NVS) causing crash** - slave trying to read saved address
4. **Missing library** - WebServer or Preferences not available

## ✅ Quick Fixes to Try

### Fix 1: Disable WiFi/WebServer Config Mode
The slave has config web UI code that might be interfering. Since we're using it as Modbus-only, let's disable that.

### Fix 2: Remove Preferences Dependency
The slave tries to load address from NVS. If NVS is corrupt or not initialized, it might crash.

### Fix 3: Simplify to Bare Minimum
Strip slave down to just:
- Serial initialization
- Modbus slave init
- Loop calling modbus.update()

## 🔧 Immediate Action

**Reboot slave and capture FULL boot sequence** to see where it's crashing:
```bash
# Unplug slave USB, plug back in, then immediately run:
pio device monitor --baud 115200 --port /dev/cu.usbmodem241201
```

Look for:
- ESP32 ROM bootloader messages
- Crash dumps or exceptions
- Where the boot sequence stops

## 📝 Notes

- Wiring hasn't changed since last night when it worked
- Code was changed today (restored from test version)
- May have introduced bug in restoration
