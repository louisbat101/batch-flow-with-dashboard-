# Slave Configuration Guide

## How to Configure Number of Active Slaves

The master controller can be configured to poll only the slaves you have connected, preventing phantom board detection from RS-485 noise/echo.

### Configuration File: `esp32-c3-master/src/config.h`

```cpp
// ⚠️  CONFIGURATION: Set how many slaves are actually connected
//    Master will only poll addresses 1 through ACTIVE_SLAVE_COUNT
//    Example: If you only have Slave 1 connected, set this to 1
//    This prevents polling phantom/disconnected slaves
#define ACTIVE_SLAVE_COUNT  1     // Number of slaves actually connected (change this!)
```

### Examples:

**1 Slave Connected (Address 1):**
```cpp
#define ACTIVE_SLAVE_COUNT  1
```
- Master polls: Slave 1 only
- Dashboard shows: Only Slave 1 (if online)

**3 Slaves Connected (Addresses 1, 2, 3):**
```cpp
#define ACTIVE_SLAVE_COUNT  3
```
- Master polls: Slaves 1, 2, 3
- Dashboard shows: Slaves 1, 2, 3 (only online ones)

**10 Slaves Connected (Addresses 1-10):**
```cpp
#define ACTIVE_SLAVE_COUNT  10
```
- Master polls: Slaves 1 through 10
- Dashboard shows: All detected slaves

### How It Works

1. **Polling Loop**: Master only polls addresses `1` through `ACTIVE_SLAVE_COUNT`
2. **Data Validation**: Before marking a slave as online, the master validates:
   - Valve state must be 0 (closed) or 1 (open)
   - Invalid data (noise/echo) is rejected
3. **Timeout**: Slaves are marked offline after 15 seconds of no valid response

### Benefits

- ✅ **No Phantom Boards**: Prevents RS-485 noise from creating fake slaves
- ✅ **Faster Polling**: Less time wasted polling empty addresses
- ✅ **Cleaner Dashboard**: Only shows real, connected stations
- ✅ **Better Performance**: ESP32 has more time for web server and other tasks

### Serial Output Example

When configured for 1 slave:
```
═════════════════════════════════════════════════════
         BATCH FLOW MASTER CONTROLLER
═════════════════════════════════════════════════════
[Config] Active Slaves: 1 (addresses 1-1)
[Config] RS-485: 9600 baud, RX=9, TX=21, RD=20
═════════════════════════════════════════════════════

[Poll] Checking slave 1...
[Poll] Slave 1: ✓ ONLINE (VALIDATED), Pulses=0, Valve=CLOSED
```

### When to Change

- **Add a new slave board**: Increase `ACTIVE_SLAVE_COUNT`
- **Remove a slave board**: Decrease `ACTIVE_SLAVE_COUNT`
- **Temporarily disable slaves**: Don't change count, they'll just show as offline

### After Changing

1. Edit `/esp32-c3-master/src/config.h`
2. Upload firmware: `pio run -e esp32c3-master -t upload`
3. Reboot master controller
4. Check serial output to confirm new configuration

---

**Note**: The system always reserves slots 1-10 for safety (chemical dispensing). Addresses never change, but the master only actively polls the slaves you configure.
