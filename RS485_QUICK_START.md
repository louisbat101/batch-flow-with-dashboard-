# RS-485 Quick Start Checklist ✅

## Before You Start
- [ ] Read RS485_SETUP_GUIDE.md (comprehensive guide)
- [ ] Read ESC3E05_PIN_CORRECTION.md (pin fix details)
- [ ] Have Blue, Brown, Black wires ready
- [ ] Both ESP32 boards connected to computer

---

## Step 1: Verify Pin Configuration

### Master Board (Custom Build)
```cpp
// esp32-c3-master/src/config.h
#define RS485_RXD_PIN     9       ✅
#define RS485_TXD_PIN     21      ✅
#define RS485_RD_PIN      20      ✅
```

### Slave Board (ESC3E05)
```cpp
// esp32-c3-slave/src/slave_config.h
#define RS485_RXD_PIN     20      ✅ (NOT 9!)
#define RS485_TXD_PIN     21      ✅
#define RS485_RD_PIN      9       ✅ (NOT 20!)
```

- [ ] Master pins verified
- [ ] Slave pins verified (remember: GPIO20=RX!)

---

## Step 2: Physical Wiring

### Connect RS-485 Terminals
```
Master A   ←→   Slave A   (Blue wire)
Master B   ←→   Slave B   (Brown wire)
Master GND ←→   Slave GND (Black wire)
```

- [ ] Blue wire: A to A
- [ ] Brown wire: B to B  
- [ ] Black wire: GND to GND
- [ ] Wires firmly connected
- [ ] Cable length < 3 meters (no termination needed)

---

## Step 3: Upload Firmware

### Master
```bash
cd esp32-c3-master
pio run -e esp32c3-master -t upload --upload-port /dev/cu.usbmodem241201
```
- [ ] Master upload successful

### Slave
```bash
cd esp32-c3-slave
pio run -e esp32c3 -t upload --upload-port /dev/cu.usbmodem241301
```
- [ ] Slave upload successful

---

## Step 4: Test Communication

### Monitor Slave First
```bash
pio device monitor -p /dev/cu.usbmodem241301 -b 115200
```

**Look for:**
```
[MB] RX: 8 bytes available
[MB] RX frame: 8 bytes - 01 03 00 00 00 04 44 09
[MB] Sending 13 bytes: 01 03 08...
[MB] TX complete
```

- [ ] Slave receiving 8-byte frames (not fragmented)
- [ ] Slave sending responses
- [ ] No CRC errors

### Monitor Master
```bash
pio device monitor -p /dev/cu.usbmodem241201 -b 115200
```

**Look for:**
```
[Modbus] ✓ SUCCESS! Got 4 registers
[Poll] Slave 1: ✓ ONLINE, Pulses=0, Valve=CLOSED
```

- [ ] Master shows SUCCESS (not timeout 0xE2)
- [ ] Slave 1 shows ONLINE
- [ ] Data displayed correctly

---

## Step 5: Web Dashboard

### Connect to Master WiFi
- SSID: `BatchFlow-Master`
- Password: `batchflow123`

### Open Dashboard
- URL: http://192.168.4.1

**Check:**
- [ ] Dashboard loads
- [ ] Board 1 shows GREEN (online)
- [ ] Status shows "Pulses: 0, Valve: CLOSED"

---

## ❌ Troubleshooting

### Slave receives ZERO bytes
- Check: Is GPIO20 set as RX? (not GPIO9!)
- Check: Are wires connected?
- Try: Swap A and B wires

### Frames are fragmented (1-3 bytes)
- Check: `delay(5)` present in slave's update() function?
- Fix: Add 5ms delay after detecting Serial1.available()

### CRC errors on every frame
- Check: CRC byte order (little-endian)
- Fix: `frame[len-1] << 8 | frame[len-2]`

### Slave receives but doesn't respond
- Check: RD pin toggling (HIGH for TX, LOW for RX)
- Check: `digitalWrite(RS485_RD_PIN, HIGH)` before send

### Master shows timeout (0xE2)
- Check: Did slave send response? (monitor slave)
- Check: Master RD pin correct? (GPIO20 for custom build)
- Try: Reboot both boards

---

## 🎉 Success Criteria

- [x] Slave receives complete 8-byte frames
- [x] No CRC errors
- [x] Slave sends 13-byte responses
- [x] Master shows "✓ SUCCESS!"
- [x] Dashboard shows Slave 1 ONLINE
- [x] No timeout errors (0xE2)

**If all checked: RS-485 is working! 🚀**

---

## 📝 Common Mistakes

1. ❌ Using GPIO9 as RX on ESC3E05 (docs are wrong!)
2. ❌ Forgetting 5ms frame buffering delay
3. ❌ Wrong CRC byte order (big-endian instead of little)
4. ❌ Not toggling RD pin for transmit
5. ❌ Crossed A/B wires
6. ❌ Missing common GND connection

---

## 📞 Quick Help

**Symptom** → **Solution**
- No RX → Check pin GPIO20 (ESC3E05)
- Fragments → Add delay(5)
- CRC error → Swap byte order
- No response → Toggle RD pin
- Timeout → Check all wiring

---

*Last Updated: May 23, 2026*  
*For detailed info: RS485_SETUP_GUIDE.md*
