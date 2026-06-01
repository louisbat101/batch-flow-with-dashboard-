# Batch Flow System - Teensy 4.1 Master Board

## ✅ STATUS: OPERATIONAL

### Hardware
- **Device**: Teensy 4.1 microcontroller
- **Port**: /dev/cu.usbmodem175441501
- **Firmware**: RS-485 Modbus RTU Master (52.2 KB)
- **Upload Protocol**: Teensy CLI (automatic via PlatformIO)

### Pinout (Teensy 4.1)
- **Pin 9** (RX2): RS-485 Receive data
- **Pin 10** (TX2): RS-485 Transmit data  
- **Pin 11**: DE/RE (Direction/Enable control)
- **USB**: Serial debug output @ 115200 baud

### RS-485 Configuration
- **Baud Rate**: 9600 bps
- **Protocol**: Modbus RTU
- **Function Code**: FC04 (Read Input Registers)
- **Slave Polling**: Addresses 1, 2, 3, 4
- **Poll Interval**: 1 second
- **Response Timeout**: 300ms
- **Direction Control**: Automatic (High for TX, Low for RX)

### Modbus Polling Cycle
```
Every 1 second:
  └─ Query Board 1
  └─ Query Board 2  
  └─ Query Board 3
  └─ Query Board 4
  └─ Output status summary
```

### Current Output
```
=== Teensy 4.1 RS-485 Master ===
Polling boards 1-4 on RS-485...

[RS485] Board 1: No response
[RS485] Board 2: No response
[RS485] Board 3: No response
[RS485] Board 4: No response
```

(Status: "No response" is expected when slaves are disconnected)

### Next Steps: Connect Slave Boards

#### Option 1: ESP32-C3 Slave Board
1. Ensure ESP32-C3 slave is programmed with Modbus slave firmware
2. Wire RS-485 connection:
   - A (Non-inv) → Teensy Pin 9
   - B (Inv) → Teensy Pin 10
3. Connect to port `/dev/cu.usbmodem241201` (slave device)
4. Monitor Teensy output - should show "[RS485] Board X OK"

#### Option 2: Multiple Slaves
- Configure each slave for different Modbus address (1-4)
- Wire all slaves to same RS-485 bus
- Teensy will automatically poll and report status

### Verification Checklist
- [x] Teensy 4.1 device boots successfully
- [x] Serial output shows "Teensy 4.1 RS-485 Master"
- [x] Board polling cycle running (every 1 second)
- [x] CRC16 validation implemented
- [x] RS-485 direction control working
- [ ] Slave board 1 responding (pending connection)
- [ ] Slave board 2 responding (pending connection)
- [ ] Slave board 3 responding (pending connection)
- [ ] Slave board 4 responding (pending connection)

### Build Commands

**Compile only:**
```bash
cd '/Users/louishome/working projects/batch flow/teensy-master'
pio run -e teensy41
```

**Compile and upload:**
```bash
pio run -e teensy41 -t upload
```

**Monitor serial output:**
```bash
pio device monitor -p /dev/cu.usbmodem* --baud 115200
```

### Troubleshooting

**"No response" from boards:**
- Verify RS-485 wiring is correct (A/B connections)
- Check slave device is powered and running
- Verify slave address is 1-4
- Check for proper termination resistors on RS-485 bus

**Garbled output:**
- Verify baud rate is 9600 on serial devices
- Check USB cable quality
- Ensure proper shielding on RS-485 lines

**Upload fails:**
- Press Program button on Teensy before upload
- Verify teensy_loader_cli is installed: `which teensy_loader_cli`
- Try: `brew install teensy_loader_cli`

### Code Highlights

**CRC16 Calculation:**
- Implements Modbus CRC16 polynomial (0xA001)
- Used for request/response validation

**Non-blocking Polling:**
- Uses `yield()` calls to allow system tasks
- 300ms timeout per board prevents blocking
- Automatic DE/RE line control via GPIO 11

**Modbus Request Format (FC04):**
```
[Slave Addr] [FC 0x04] [Reg Hi] [Reg Lo] [Count Hi] [Count Lo] [CRC Lo] [CRC Hi]
   1 byte      1 byte    1 byte  1 byte   1 byte     1 byte     1 byte  1 byte
```

---
**Last Updated**: June 1, 2026
**Hardware**: Teensy 4.1 IMXRT1062
**Firmware Size**: 52.2 KB (2.6% of 7.75 MB flash)
**RAM Usage**: <10 KB (dynamic, dependent on runtime state)
