# Summary: Teensy ↔ C3 WebUI Architecture

## Status
✅ **Teensy Master Firmware**: Updated
- Queries RS-485 slaves (Board 2 responding)
- Ready to send status via UART to C3
- Pins: UART1 (Serial1) on Pins 1(RX) ← C3 GPIO 21, Pin 2(TX) → C3 GPIO 20

## Next: C3 WebServer Firmware

The ESP32-C3 needs to be updated to:

1. **Receive UART data** from Teensy (GPIO 20=RX, GPIO 21=TX @ 115200 baud)
2. **Parse status messages** using the protocol defined in `TEENSY_C3_UART_PROTOCOL.md`
3. **Display real-time status** in the web UI
4. **Handle user input** from the tablet and send control commands back to Teensy

**Architecture**:
```
Teensy (Master) 
    ↓ UART @ 115200
ESP32-C3 (WebServer)
    ↓ WiFi
Tablet Browser
```

## Files Ready
- ✅ `TEENSY_C3_UART_PROTOCOL.md` - Communication protocol
- ✅ `/teensy-master/src/main.cpp` - Teensy firmware (compiled & uploaded)
- ⏳ Need: C3 firmware update

## Next Action
Create C3 firmware that:
1. Uses GPIO 20 (RX from Teensy) and GPIO 21 (TX to Teensy)
2. Parses incoming status messages from Teensy
3. Updates the web UI /api/boards/status endpoint
4. Sends control commands back to Teensy when user submits forms
5. Maintains the existing web UI from `/esp32-c3-master/data/`
