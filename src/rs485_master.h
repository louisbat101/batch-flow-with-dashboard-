#pragma once
/*
 *  RS-485 Master Driver for ESP32-WROOM
 *
 *  Sends commands to C3 slave nodes and receives responses.
 *  Replaces direct Flowmeter / Valve access.
 *
 *  Each slave node has an address (1 = Line 1, 2 = Line 2).
 *  The master sends a request and waits for a response within a timeout.
 */

#include <Arduino.h>
#include "config.h"
#include "protocol.h"

class RS485Master {
public:
    // ── Initialise the RS-485 bus ────────────────────
    static void begin() {
        pinMode(RS485_DE, OUTPUT);
        digitalWrite(RS485_DE, LOW);        // receive mode
        Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);
        // Flush any garbage
        while (Serial2.available()) Serial2.read();
        Serial.println("[RS485] Master initialised (8N1)");
    }

    // ── Switch parity (for Modbus valve debugging) ──
    static void setParity8E1() {
        Serial2.end();
        Serial2.begin(RS485_BAUD, SERIAL_8E1, RS485_RX, RS485_TX);
        while (Serial2.available()) Serial2.read();
        Serial.println("[RS485] Switched to 8E1 (even parity)");
    }
    static void setParity8N1() {
        Serial2.end();
        Serial2.begin(RS485_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);
        while (Serial2.available()) Serial2.read();
        Serial.println("[RS485] Switched to 8N1 (no parity)");
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Custom Protocol (for C3 flowmeter nodes)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Send a raw frame ────────────────────────────
    static void sendFrame(uint8_t addr, uint8_t cmd,
                          uint8_t d0 = 0, uint8_t d1 = 0,
                          uint8_t d2 = 0, uint8_t d3 = 0)
    {
        uint8_t buf[PROTO_FRAME_SIZE];
        protoBuildFrame(buf, addr, cmd, d0, d1, d2, d3);

        // Flush RX buffer before sending
        while (Serial2.available()) Serial2.read();

        digitalWrite(RS485_DE, HIGH);       // transmit mode
        delayMicroseconds(200);
        Serial2.write(buf, PROTO_FRAME_SIZE);
        Serial2.flush();
        delayMicroseconds(200);
        digitalWrite(RS485_DE, LOW);        // back to receive
    }

    // ── Wait for a response frame (blocking) ────────
    //    Returns true if valid frame received
    static bool waitResponse(uint8_t *resp, unsigned long timeoutMs = RS485_TIMEOUT_MS) {
        unsigned long start = millis();
        int pos = 0;

        while ((millis() - start) < timeoutMs) {
            while (Serial2.available()) {
                uint8_t b = Serial2.read();
                if (pos == 0 && b != PROTO_SYNC) continue;
                resp[pos++] = b;
                if (pos >= PROTO_FRAME_SIZE) {
                    if (protoValidate(resp)) return true;
                    // Bad checksum – try re-syncing
                    pos = 0;
                }
            }
            delayMicroseconds(100);
        }
        return false;   // timeout
    }

    // ── High-level: send + receive ──────────────────
    static bool sendAndReceive(uint8_t addr, uint8_t cmd,
                               uint8_t *resp,
                               uint8_t d0 = 0, uint8_t d1 = 0,
                               uint8_t d2 = 0, uint8_t d3 = 0)
    {
        sendFrame(addr, cmd, d0, d1, d2, d3);
        return waitResponse(resp);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Modbus RTU (for TONHE proportional valves)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Modbus CRC16 ────────────────────────────────
    static uint16_t modbusCRC16(const uint8_t *data, uint16_t len) {
        uint16_t crc = 0xFFFF;
        for (uint16_t i = 0; i < len; i++) {
            crc ^= (uint16_t)data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 0x0001) {
                    crc >>= 1;
                    crc ^= 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }

    // ── Send Modbus RTU Write Single Register (0x06) ─
    //    addr: slave Modbus address (1-247)
    //    reg:  register address
    //    value: value to write
    //    Returns true if the valve echoed back correctly
    static bool modbusWriteRegister(uint8_t addr, uint16_t reg, uint16_t value) {
        uint8_t frame[8];
        frame[0] = addr;
        frame[1] = 0x06;               // function code: Write Single Register
        frame[2] = (reg >> 8) & 0xFF;  // register high
        frame[3] = reg & 0xFF;         // register low
        frame[4] = (value >> 8) & 0xFF; // value high
        frame[5] = value & 0xFF;        // value low
        uint16_t crc = modbusCRC16(frame, 6);
        frame[6] = crc & 0xFF;          // CRC low first (Modbus RTU)
        frame[7] = (crc >> 8) & 0xFF;   // CRC high

        // Hex dump of TX frame
        Serial.printf("[MODBUS TX] ");
        for (int i = 0; i < 8; i++) Serial.printf("%02X ", frame[i]);
        Serial.println();

        // Flush RX
        while (Serial2.available()) Serial2.read();

        // Modbus requires 3.5 char silence before frame (at 9600 baud ≈ 4ms)
        delay(5);

        digitalWrite(RS485_DE, HIGH);    // transmit
        delayMicroseconds(500);          // let DE settle
        Serial2.write(frame, 8);
        Serial2.flush();
        delayMicroseconds(500);          // ensure last byte fully out
        digitalWrite(RS485_DE, LOW);     // receive

        // Wait for echo response (valve echoes same 8 bytes on success)
        // Use longer timeout for slow valves (500ms)
        unsigned long tOut = 500;
        unsigned long start = millis();
        uint8_t resp[8];
        int pos = 0;
        bool gotAnyByte = false;
        while ((millis() - start) < tOut) {
            while (Serial2.available()) {
                uint8_t b = Serial2.read();
                if (!gotAnyByte) {
                    gotAnyByte = true;
                    Serial.printf("[MODBUS RX] ");
                }
                Serial.printf("%02X ", b);
                resp[pos++] = b;
                if (pos >= 8) {
                    Serial.println();
                    // Validate: addr + func + reg + value should match
                    if (resp[0] == addr && resp[1] == 0x06) {
                        uint16_t respCrc = resp[6] | (resp[7] << 8);
                        if (respCrc == modbusCRC16(resp, 6)) {
                            Serial.printf("[MODBUS] Valve %d – OK!\n", addr);
                            return true;   // success
                        }
                    }
                    // Error response (func | 0x80)
                    if (resp[0] == addr && resp[1] == 0x86) {
                        Serial.printf("[MODBUS] Valve %d error code: 0x%02X\n", addr, resp[2]);
                        return false;
                    }
                    pos = 0;  // bad frame, keep trying
                    gotAnyByte = false;
                }
            }
            delayMicroseconds(100);
        }
        if (gotAnyByte) Serial.println();
        Serial.printf("[MODBUS] Valve %d – no response! (waited %lums)\n", addr, tOut);
        return false;
    }

    // ── Read Modbus Holding Register (0x03) ─────────
    //    Returns true if response valid, puts value in *outValue
    static bool modbusReadRegister(uint8_t addr, uint16_t reg, uint16_t *outValue) {
        uint8_t frame[8];
        frame[0] = addr;
        frame[1] = 0x03;               // function code: Read Holding Registers
        frame[2] = (reg >> 8) & 0xFF;
        frame[3] = reg & 0xFF;
        frame[4] = 0x00;               // quantity high
        frame[5] = 0x01;               // quantity low (1 register)
        uint16_t crc = modbusCRC16(frame, 6);
        frame[6] = crc & 0xFF;
        frame[7] = (crc >> 8) & 0xFF;

        while (Serial2.available()) Serial2.read();
        delay(5);
        digitalWrite(RS485_DE, HIGH);
        delayMicroseconds(500);
        Serial2.write(frame, 8);
        Serial2.flush();
        delayMicroseconds(500);
        digitalWrite(RS485_DE, LOW);

        // Response: addr(1) + func(1) + byteCount(1) + data(2) + crc(2) = 7 bytes
        unsigned long start = millis();
        uint8_t resp[16];
        int pos = 0;
        bool gotAnyByte = false;
        while ((millis() - start) < 200) {
            while (Serial2.available()) {
                uint8_t b = Serial2.read();
                if (!gotAnyByte) {
                    gotAnyByte = true;
                    Serial.printf("[MODBUS RD RX] ");
                }
                Serial.printf("%02X ", b);
                resp[pos++] = b;
                if (pos >= 7) {
                    Serial.println();
                    if (resp[0] == addr && resp[1] == 0x03 && resp[2] == 0x02) {
                        uint16_t respCrc = resp[5] | (resp[6] << 8);
                        if (respCrc == modbusCRC16(resp, 5)) {
                            *outValue = (resp[3] << 8) | resp[4];
                            return true;
                        }
                    }
                    return false;
                }
            }
            delayMicroseconds(100);
        }
        return false;
    }

    // ── Scan Modbus addresses 1-247 (quick ping via read reg 0x0001) ──
    //    Calls callback(addr) for each found device
    static int modbusScan(uint8_t startAddr, uint8_t endAddr, void (*foundCb)(uint8_t addr) = nullptr) {
        int found = 0;
        Serial.printf("[MODBUS SCAN] Scanning addresses %d-%d ...\n", startAddr, endAddr);
        for (uint8_t a = startAddr; a <= endAddr; a++) {
            uint16_t val = 0;
            if (modbusReadRegister(a, 0x0001, &val)) {
                Serial.printf("[MODBUS SCAN] Found device at addr %d (reg 0x0001 = 0x%04X)\n", a, val);
                if (foundCb) foundCb(a);
                found++;
            }
        }
        Serial.printf("[MODBUS SCAN] Done – %d device(s) found\n", found);
        return found;
    }

    // ── Send Modbus RTU Write Multiple Registers (0x10) ─
    //    Some valves only respond to 0x10, not 0x06
    static bool modbusWriteMultipleRegisters(uint8_t addr, uint16_t reg, uint16_t value) {
        // Frame: addr(1) + func(1) + startReg(2) + qty(2) + byteCount(1) + data(2) + CRC(2) = 11 bytes
        uint8_t frame[11];
        frame[0] = addr;
        frame[1] = 0x10;                // function code: Write Multiple Registers
        frame[2] = (reg >> 8) & 0xFF;
        frame[3] = reg & 0xFF;
        frame[4] = 0x00;                // quantity high
        frame[5] = 0x01;                // quantity low (1 register)
        frame[6] = 0x02;                // byte count (2 bytes per register)
        frame[7] = (value >> 8) & 0xFF;
        frame[8] = value & 0xFF;
        uint16_t crc = modbusCRC16(frame, 9);
        frame[9]  = crc & 0xFF;
        frame[10] = (crc >> 8) & 0xFF;

        Serial.printf("[MODBUS TX 0x10] ");
        for (int i = 0; i < 11; i++) Serial.printf("%02X ", frame[i]);
        Serial.println();

        while (Serial2.available()) Serial2.read();
        delay(5);
        digitalWrite(RS485_DE, HIGH);
        delayMicroseconds(500);
        Serial2.write(frame, 11);
        Serial2.flush();
        delayMicroseconds(500);
        digitalWrite(RS485_DE, LOW);

        // Response: addr(1) + func(1) + startReg(2) + qty(2) + CRC(2) = 8 bytes
        unsigned long start = millis();
        uint8_t resp[8];
        int pos = 0;
        bool gotAnyByte = false;
        while ((millis() - start) < 500) {
            while (Serial2.available()) {
                uint8_t b = Serial2.read();
                if (!gotAnyByte) { gotAnyByte = true; Serial.printf("[MODBUS 0x10 RX] "); }
                Serial.printf("%02X ", b);
                resp[pos++] = b;
                if (pos >= 8) {
                    Serial.println();
                    if (resp[0] == addr && resp[1] == 0x10) {
                        uint16_t respCrc = resp[6] | (resp[7] << 8);
                        if (respCrc == modbusCRC16(resp, 6)) {
                            Serial.printf("[MODBUS] Valve %d – 0x10 OK!\n", addr);
                            return true;
                        }
                    }
                    if (resp[0] == addr && resp[1] == 0x90) {
                        Serial.printf("[MODBUS] Valve %d 0x10 error: 0x%02X\n", addr, resp[2]);
                        return false;
                    }
                    pos = 0;
                    gotAnyByte = false;
                }
            }
            delayMicroseconds(100);
        }
        if (gotAnyByte) Serial.println();
        Serial.printf("[MODBUS] Valve %d – no 0x10 response! (waited 500ms)\n", addr);
        return false;
    }

    // ── Write Single Coil (0x05) ───────────────────
    //    value: 0xFF00 = ON, 0x0000 = OFF
    static bool modbusWriteCoil(uint8_t addr, uint16_t coil, uint16_t value) {
        uint8_t frame[8];
        frame[0] = addr;
        frame[1] = 0x05;               // function code: Write Single Coil
        frame[2] = (coil >> 8) & 0xFF;
        frame[3] = coil & 0xFF;
        frame[4] = (value >> 8) & 0xFF; // 0xFF or 0x00
        frame[5] = value & 0xFF;        // 0x00
        uint16_t crc = modbusCRC16(frame, 6);
        frame[6] = crc & 0xFF;
        frame[7] = (crc >> 8) & 0xFF;

        Serial.printf("[MODBUS TX 0x05] ");
        for (int i = 0; i < 8; i++) Serial.printf("%02X ", frame[i]);
        Serial.println();

        while (Serial2.available()) Serial2.read();
        delay(5);
        digitalWrite(RS485_DE, HIGH);
        delayMicroseconds(500);
        Serial2.write(frame, 8);
        Serial2.flush();
        delayMicroseconds(500);
        digitalWrite(RS485_DE, LOW);

        unsigned long start = millis();
        uint8_t resp[8];
        int pos = 0;
        bool gotAnyByte = false;
        while ((millis() - start) < 500) {
            while (Serial2.available()) {
                uint8_t b = Serial2.read();
                if (!gotAnyByte) { gotAnyByte = true; Serial.printf("[MODBUS 0x05 RX] "); }
                Serial.printf("%02X ", b);
                resp[pos++] = b;
                if (pos >= 8) {
                    Serial.println();
                    if (resp[0] == addr && resp[1] == 0x05) {
                        uint16_t respCrc = resp[6] | (resp[7] << 8);
                        if (respCrc == modbusCRC16(resp, 6)) {
                            Serial.printf("[MODBUS] Coil write OK!\n");
                            return true;
                        }
                    }
                    if (resp[0] == addr && resp[1] == 0x85) {
                        Serial.printf("[MODBUS] Coil error: 0x%02X\n", resp[2]);
                        return false;
                    }
                    pos = 0;
                    gotAnyByte = false;
                }
            }
            delayMicroseconds(100);
        }
        if (gotAnyByte) Serial.println();
        Serial.printf("[MODBUS] Coil write – no response (500ms)\n");
        return false;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  TONHE Proportional Valve Protocol
    //  ─────────────────────────────────────────────────
    //  Reg 0x0000 = Device address (1-254)
    //  Reg 0x0001 = Position SET   (int16, ÷100 = degrees)
    //               0     = 0.00°  = FULLY OPEN
    //               9000  = 90.00° = FULLY CLOSED
    //               -32768 (0x8000) = power-on default (no action)
    //               Range: -10500 to 19500
    //  Reg 0x0002 = Error status   (0=OK, 1=blocked)
    //  Reg 0x0003 = Position FEEDBACK (same scale, read-only)
    //
    //  Only func 0x03 (read) and 0x10 (write) supported.
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    static constexpr uint16_t VALVE_REG_POSITION = 0x0001;
    static constexpr uint16_t VALVE_REG_ERROR    = 0x0002;
    static constexpr uint16_t VALVE_REG_FEEDBACK = 0x0003;
    static constexpr uint16_t VALVE_POS_OPEN     = 9000;    // 90° = OPEN  (reversed install)
    static constexpr uint16_t VALVE_POS_CLOSED   = 0;       // 0°  = CLOSED (reversed install)

    // ── Open valve = set position to 90 degrees ─────
    static bool modbusOpenValve(uint8_t addr) {
        Serial.printf("[VALVE] OPEN addr=%d  → reg 0x0001 = 9000 (90°)\n", addr);
        return modbusWriteMultipleRegisters(addr, VALVE_REG_POSITION, VALVE_POS_OPEN);
    }

    // ── Close valve = set position to 0 degrees ─────
    static bool modbusCloseValve(uint8_t addr) {
        Serial.printf("[VALVE] CLOSE addr=%d → reg 0x0001 = 0 (0°)\n", addr);
        return modbusWriteMultipleRegisters(addr, VALVE_REG_POSITION, VALVE_POS_CLOSED);
    }

    // ── Set valve to arbitrary angle (×100) ─────────
    //    e.g. setValvePosition(addr, 4500) = 45.00°
    static bool modbusSetValvePosition(uint8_t addr, int16_t angleTimes100) {
        Serial.printf("[VALVE] SET addr=%d → %d (%.2f°)\n", addr, angleTimes100, angleTimes100 / 100.0f);
        return modbusWriteMultipleRegisters(addr, VALVE_REG_POSITION, (uint16_t)angleTimes100);
    }

    // ── Read valve position feedback ────────────────
    static bool modbusReadValvePosition(uint8_t addr, int16_t *angleTimes100) {
        uint16_t raw = 0;
        bool ok = modbusReadRegister(addr, VALVE_REG_FEEDBACK, &raw);
        if (ok) {
            *angleTimes100 = (int16_t)raw;
            Serial.printf("[VALVE] FEEDBACK addr=%d → %d (%.2f°)\n", addr, *angleTimes100, *angleTimes100 / 100.0f);
        }
        return ok;
    }

    // ── Read valve error status ─────────────────────
    static bool modbusReadValveError(uint8_t addr, uint16_t *errCode) {
        bool ok = modbusReadRegister(addr, VALVE_REG_ERROR, errCode);
        if (ok) Serial.printf("[VALVE] ERROR addr=%d → %d (%s)\n", addr, *errCode, *errCode == 0 ? "OK" : "BLOCKED");
        return ok;
    }

    // ── Clear valve blocked error ───────────────────
    static bool modbusValveClearError(uint8_t addr) {
        Serial.printf("[VALVE] CLEAR ERROR addr=%d\n", addr);
        return modbusWriteMultipleRegisters(addr, VALVE_REG_ERROR, 0);
    }

    // ── Read Coil Status (0x01) ─────────────────────
    static bool modbusReadCoil(uint8_t addr, uint16_t coil, bool *outVal) {
        uint8_t frame[8];
        frame[0] = addr;
        frame[1] = 0x01;               // Read Coils
        frame[2] = (coil >> 8) & 0xFF;
        frame[3] = coil & 0xFF;
        frame[4] = 0x00;               // quantity high
        frame[5] = 0x01;               // quantity low
        uint16_t crc = modbusCRC16(frame, 6);
        frame[6] = crc & 0xFF;
        frame[7] = (crc >> 8) & 0xFF;

        while (Serial2.available()) Serial2.read();
        delay(5);
        digitalWrite(RS485_DE, HIGH);
        delayMicroseconds(500);
        Serial2.write(frame, 8);
        Serial2.flush();
        delayMicroseconds(500);
        digitalWrite(RS485_DE, LOW);

        // Response: addr(1) + func(1) + byteCount(1) + data(1) + crc(2) = 6 bytes
        unsigned long start = millis();
        uint8_t resp[16];
        int pos = 0;
        bool gotAnyByte = false;
        while ((millis() - start) < 300) {
            while (Serial2.available()) {
                uint8_t b = Serial2.read();
                if (!gotAnyByte) { gotAnyByte = true; Serial.printf("[MODBUS 0x01 RX] "); }
                Serial.printf("%02X ", b);
                resp[pos++] = b;
                if (pos >= 6) {
                    Serial.println();
                    if (resp[0] == addr && resp[1] == 0x01 && resp[2] == 0x01) {
                        uint16_t respCrc = resp[4] | (resp[5] << 8);
                        if (respCrc == modbusCRC16(resp, 4)) {
                            *outVal = (resp[3] & 0x01) != 0;
                            return true;
                        }
                    }
                    return false;
                }
            }
            delayMicroseconds(100);
        }
        return false;
    }

    // ── Read Input Registers (0x04) ─────────────────
    static bool modbusReadInputRegister(uint8_t addr, uint16_t reg, uint16_t *outValue) {
        uint8_t frame[8];
        frame[0] = addr;
        frame[1] = 0x04;               // Read Input Registers
        frame[2] = (reg >> 8) & 0xFF;
        frame[3] = reg & 0xFF;
        frame[4] = 0x00;
        frame[5] = 0x01;
        uint16_t crc = modbusCRC16(frame, 6);
        frame[6] = crc & 0xFF;
        frame[7] = (crc >> 8) & 0xFF;

        while (Serial2.available()) Serial2.read();
        delay(5);
        digitalWrite(RS485_DE, HIGH);
        delayMicroseconds(500);
        Serial2.write(frame, 8);
        Serial2.flush();
        delayMicroseconds(500);
        digitalWrite(RS485_DE, LOW);

        unsigned long start = millis();
        uint8_t resp[16];
        int pos = 0;
        while ((millis() - start) < 300) {
            while (Serial2.available()) {
                resp[pos++] = Serial2.read();
                if (pos >= 7) {
                    if (resp[0] == addr && resp[1] == 0x04 && resp[2] == 0x02) {
                        uint16_t respCrc = resp[5] | (resp[6] << 8);
                        if (respCrc == modbusCRC16(resp, 5)) {
                            *outValue = (resp[3] << 8) | resp[4];
                            return true;
                        }
                    }
                    return false;
                }
            }
            delayMicroseconds(100);
        }
        return false;
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  Flowmeter convenience wrappers (custom protocol → C3)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Ping a slave (returns true if alive) ────────
    static bool ping(uint8_t addr) {
        uint8_t resp[PROTO_FRAME_SIZE];
        if (sendAndReceive(addr, CMD_PING, resp)) {
            return (resp[2] == CMD_PONG);
        }
        return false;
    }

    // ── Reset pulse counter on slave ────────────────
    static bool resetPulses(uint8_t addr) {
        uint8_t resp[PROTO_FRAME_SIZE];
        if (sendAndReceive(addr, CMD_FLOW_RESET, resp)) {
            return (resp[2] == CMD_FLOW_DATA);
        }
        Serial.printf("[RS485] resetPulses(%d) – no response!\n", addr);
        return false;
    }

    // ── Poll pulse count from slave (channel A) ──────
    //    Returns pulse count, or 0 on failure
    static uint32_t readPulses(uint8_t addr) {
        uint8_t resp[PROTO_FRAME_SIZE];
        if (sendAndReceive(addr, CMD_FLOW_POLL, resp)) {
            if (resp[2] == CMD_FLOW_DATA) {
                return protoUnpackU32(&resp[3]);
            }
        }
        Serial.printf("[RS485] readPulses(%d) – no response!\n", addr);
        return 0;
    }

    // ── Poll pulse count from slave (channel B) ─────
    static uint32_t readPulsesB(uint8_t addr) {
        uint8_t resp[PROTO_FRAME_SIZE];
        if (sendAndReceive(addr, CMD_FLOW_POLL_B, resp)) {
            if (resp[2] == CMD_FLOW_DATA_B) {
                return protoUnpackU32(&resp[3]);
            }
        }
        Serial.printf("[RS485] readPulsesB(%d) – no response!\n", addr);
        return 0;
    }

    // ── Reset pulse counter B on slave ──────────────
    static bool resetPulsesB(uint8_t addr) {
        uint8_t resp[PROTO_FRAME_SIZE];
        if (sendAndReceive(addr, CMD_FLOW_RESET_B, resp)) {
            return (resp[2] == CMD_FLOW_DATA_B);
        }
        Serial.printf("[RS485] resetPulsesB(%d) – no response!\n", addr);
        return false;
    }

    // ── Read litres (pulses / calibration) ──────────
    static float readLitres(uint8_t addr, float pulsesPerLitre) {
        uint32_t p = readPulses(addr);
        return (float)p / pulsesPerLitre;
    }

    static float readLitresB(uint8_t addr, float pulsesPerLitre) {
        uint32_t p = readPulsesB(addr);
        return (float)p / pulsesPerLitre;
    }

    // ── Convenience aliases for Line 1 / Line 2 ────
    static bool  resetPulses1()  { return resetPulses(SLAVE1_ADDR); }
    static bool  resetPulses2()  { return resetPulses(SLAVE2_ADDR); }
    static bool  resetPulses1B() { return resetPulsesB(SLAVE1_ADDR); }
    static bool  resetPulses2B() { return resetPulsesB(SLAVE2_ADDR); }

    static uint32_t readPulses1()  { return readPulses(SLAVE1_ADDR); }
    static uint32_t readPulses2()  { return readPulses(SLAVE2_ADDR); }
    static uint32_t readPulses1B() { return readPulsesB(SLAVE1_ADDR); }
    static uint32_t readPulses2B() { return readPulsesB(SLAVE2_ADDR); }

    static float litres1(float cal)  { return readLitres(SLAVE1_ADDR, cal); }
    static float litres2(float cal)  { return readLitres(SLAVE2_ADDR, cal); }
    static float litres1B(float cal) { return readLitresB(SLAVE1_ADDR, cal); }
    static float litres2B(float cal) { return readLitresB(SLAVE2_ADDR, cal); }
};
