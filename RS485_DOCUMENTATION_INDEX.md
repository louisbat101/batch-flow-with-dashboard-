# 📚 RS-485 Documentation Index

**Complete reference for ESP32-C3 RS-485 Modbus RTU implementation**

---

## 🚀 Quick Links

| Document | Purpose | Read This If... |
|----------|---------|----------------|
| [RS485_QUICK_START.md](./RS485_QUICK_START.md) | Step-by-step checklist | You want to get started fast |
| [RS485_SETUP_GUIDE.md](./RS485_SETUP_GUIDE.md) | Complete technical guide | You want all the details |
| [ESC3E05_PIN_CORRECTION.md](./ESC3E05_PIN_CORRECTION.md) | Pin fix for ESC3E05 | You're using ESC3E05 board |
| [RS485_WIRING_DIAGRAM.md](./RS485_WIRING_DIAGRAM.md) | Visual diagrams | You want to see connections |
| [RS485_SUCCESS_SUMMARY.md](./RS485_SUCCESS_SUMMARY.md) | What works now | You want final config |

---

## 📖 Document Descriptions

### 1. RS485_QUICK_START.md
**Best for:** First-time setup  
**Contains:**
- Pre-flight checklist
- Pin verification
- Step-by-step wiring
- Upload commands
- Testing procedure
- Troubleshooting quick fixes

### 2. RS485_SETUP_GUIDE.md  
**Best for:** Deep understanding  
**Contains:**
- Complete pin documentation
- Why ESC3E05 docs are wrong
- How we discovered the fix
- Modbus RTU protocol details
- CRC byte order explanation
- Frame timing requirements
- Library recommendations
- Comprehensive troubleshooting

### 3. ESC3E05_PIN_CORRECTION.md
**Best for:** ESC3E05 users  
**Contains:**
- What the PCB says (wrong)
- What actually works (correct)
- Proof of discovery
- Usage examples
- Critical reminders

### 4. RS485_WIRING_DIAGRAM.md
**Best for:** Visual learners  
**Contains:**
- ASCII art system diagram
- Pin assignment tables
- Wire color codes
- Direction control explanation
- Modbus frame examples
- Timing diagrams
- Success indicators

### 5. RS485_SUCCESS_SUMMARY.md
**Best for:** Quick reference  
**Contains:**
- Final working configuration
- Test results
- What was fixed
- Web interface URLs
- Next steps
- Lessons learned

---

## ⚠️ CRITICAL WARNINGS

### For ESC3E05 Users:
```
The PCB silkscreen/documentation is WRONG!

Documentation says:  RD=GPIO9, RXD=GPIO20
Reality:            RD=GPIO9, RXD=GPIO20  ← SWAPPED!

ALWAYS USE:
  RS485_RXD_PIN = 20  (not 9!)
  RS485_RD_PIN  = 9   (not 20!)
```

### For Custom Builds:
Match your actual wiring - pins may differ from ESC3E05!

---

## 🎯 Recommended Reading Order

### First Time Setup:
1. Start with **RS485_QUICK_START.md**
2. Reference **RS485_WIRING_DIAGRAM.md** while wiring
3. Check **ESC3E05_PIN_CORRECTION.md** if using that board

### Troubleshooting:
1. Quick fixes in **RS485_QUICK_START.md** (bottom section)
2. Detailed diagnosis in **RS485_SETUP_GUIDE.md**
3. Visual reference in **RS485_WIRING_DIAGRAM.md**

### Understanding How It Works:
1. Read **RS485_SETUP_GUIDE.md** completely
2. Review **RS485_SUCCESS_SUMMARY.md** for context
3. Study **RS485_WIRING_DIAGRAM.md** for details

---

## 🔧 Configuration Files

### Master Configuration:
```
esp32-c3-master/src/config.h
  - RS485 pin definitions
  - WiFi settings
  - Slave polling configuration
```

### Slave Configuration:
```
esp32-c3-slave/src/slave_config.h
  - RS485 pin definitions (⚠️ corrected!)
  - Relay pin mappings
  - Default slave address
  - Flowmeter settings
```

### Modbus Implementation:
```
esp32-c3-master/src/modbus_master_lib.h
  - ModbusMaster library wrapper
  - Direction control callbacks
  
esp32-c3-slave/src/modbus_slave.h
  - Custom Modbus RTU slave
  - Frame parsing
  - CRC calculation (little-endian!)
```

---

## 📞 Quick Help Matrix

| Problem | Check Document | Section |
|---------|---------------|---------|
| No RX data | ESC3E05_PIN_CORRECTION.md | Entire document |
| Fragmented frames | RS485_SETUP_GUIDE.md | Frame Timing |
| CRC errors | RS485_SETUP_GUIDE.md | Modbus RTU Details |
| No response | RS485_SETUP_GUIDE.md | Direction Control |
| Wiring confusion | RS485_WIRING_DIAGRAM.md | Wire Colors |
| Upload errors | RS485_QUICK_START.md | Step 3 |
| Testing procedure | RS485_QUICK_START.md | Step 4 |

---

## 🎓 Learning Resources

### Inside This Project:
- Working code examples in `esp32-c3-master/` and `esp32-c3-slave/`
- ModbusMaster library integration
- Custom Modbus slave implementation
- Frame timing and buffering examples

### External References:
- ModbusMaster Library: https://github.com/4-20ma/ModbusMaster
- Modbus RTU Specification: modbus.org
- MAX485 Datasheet: Maxim Integrated
- ESP32-C3 Datasheet: Espressif Systems

---

## ✅ Verification Checklist

Before asking for help, verify:

- [ ] Read RS485_QUICK_START.md completely
- [ ] ESC3E05 using GPIO20 for RX (not GPIO9)
- [ ] 5ms delay present in slave update() function
- [ ] CRC using little-endian byte order
- [ ] RD pin toggling HIGH for TX, LOW for RX
- [ ] All three wires connected (A, B, GND)
- [ ] Both boards powered and uploaded
- [ ] Checked both serial monitors

---

## 🎉 Success Criteria

Your system is working correctly when:

✅ Slave monitor shows: `[MB] RX: 8 bytes available`  
✅ Slave monitor shows: `[MB] TX complete`  
✅ Master monitor shows: `[Modbus] ✓ SUCCESS!`  
✅ Master monitor shows: `[Poll] Slave 1: ✓ ONLINE`  
✅ Dashboard at http://192.168.4.1 shows Board 1 GREEN  
✅ No timeout errors (0xE2)  
✅ No CRC errors  

---

## 📝 Document History

- **May 23, 2026**: Initial documentation created
  - Discovered ESC3E05 pin swap issue
  - Implemented frame buffering fix
  - Corrected CRC byte order
  - Achieved full bidirectional communication

---

## 🙏 Credits

**Problem Solver:** AI Assistant (GitHub Copilot)  
**Hardware Tester:** Louis (User)  
**Hardware:** ESP32-C3 Super Mini + ESC3E05 Expansion Board  
**Key Discovery:** GPIO20 is actual RX pin on ESC3E05 (not GPIO9)  
**Time Investment:** ~7.5 hours from broken to working  

---

## 📮 Support

For issues or questions:
1. Check the troubleshooting sections in RS485_SETUP_GUIDE.md
2. Review RS485_QUICK_START.md checklist
3. Verify pin configuration in ESC3E05_PIN_CORRECTION.md
4. Compare with working config in RS485_SUCCESS_SUMMARY.md

---

**Last Updated:** May 23, 2026  
**Status:** ✅ WORKING - Full bidirectional Modbus RTU communication  
**Next Steps:** Test valve control and flowmeter integration
