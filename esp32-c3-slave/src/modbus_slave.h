#pragma once

#include <Arduino.h>
#include "slave_config.h"

// ═══════════════════════════════════════════════════════════════════════
// Modbus RTU Slave Handler
// Communicates with master over RS-485 using Modbus RTU protocol
// ═══════════════════════════════════════════════════════════════════════

class ModbusSlave {
public:
  // Modbus function codes
  static constexpr uint8_t FC_READ_COILS       = 0x01;
  static constexpr uint8_t FC_READ_INPUTS      = 0x02;
  static constexpr uint8_t FC_READ_REGS        = 0x03;
  static constexpr uint8_t FC_READ_INPUT_REGS  = 0x04;
  static constexpr uint8_t FC_WRITE_COIL       = 0x05;
  static constexpr uint8_t FC_WRITE_REG        = 0x06;
  static constexpr uint8_t FC_WRITE_COILS      = 0x0F;
  static constexpr uint8_t FC_WRITE_REGS       = 0x10;

  // Register map (Modbus addresses)
  // 0x0000-0x0003: Coils (relay states) - RW
  //   0x0000 = Relay 1
  //   0x0001 = Relay 2
  //   0x0002 = Relay 3
  //   0x0003 = Relay 4
  // 0x0100-0x0102: Input registers (read-only)
  //   0x0100 = Pulse count (high word)
  //   0x0101 = Pulse count (low word)
  //   0x0102 = Status flags

  ModbusSlave(uint8_t slaveAddr = DEFAULT_SLAVE_ADDR)
    : slaveAddress(slaveAddr), 
      coilState{false, false, false, false},
      enabled(false) {}

  void begin() {
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
    pinMode(RS485_RD_PIN, OUTPUT);
    digitalWrite(RS485_RD_PIN, LOW);  // Enable receive mode initially
    enabled = true;
    Serial.printf("[MB] Modbus slave %u on RS485 (GPIO%u RX, GPIO%u TX, GPIO%u RD)\n",
                  slaveAddress, RS485_RXD_PIN, RS485_TXD_PIN, RS485_RD_PIN);
  }

  void setSlaveAddress(uint8_t addr) {
    slaveAddress = addr;
    Serial.printf("[MB] Slave address changed to %u\n", addr);
  }

  uint8_t getSlaveAddress() const {
    return slaveAddress;
  }

  // Set relay state (from Modbus command or local control)
  void setRelayState(uint8_t relayNum, bool state) {
    if (relayNum < 4) {
      coilState[relayNum] = state;
      Serial.printf("[MB] Relay %u set to %s\n", relayNum + 1, state ? "ON" : "OFF");
    }
  }

  // Get relay state
  bool getRelayState(uint8_t relayNum) const {
    if (relayNum < 4) return coilState[relayNum];
    return false;
  }

  // Call in main loop to process incoming Modbus requests
  void update() {
    if (!enabled || !Serial1.available()) return;

    // Read incoming frame
    uint8_t frame[256];
    int frameLen = 0;
    unsigned long timeout = millis() + 100;  // 100ms timeout

    while (Serial1.available() && millis() < timeout && frameLen < 256) {
      frame[frameLen++] = Serial1.read();
    }

    if (frameLen < 4) return;  // Minimum valid frame

    // Check slave address
    if (frame[0] != slaveAddress) return;

    // Parse and respond
    processModbusFrame(frame, frameLen);
  }

  // Get relay states as bitmask (for REST API or UI)
  uint8_t getRelaysMask() const {
    uint8_t mask = 0;
    for (int i = 0; i < 4; i++) {
      if (coilState[i]) mask |= (1 << i);
    }
    return mask;
  }

  // Debug: print current coil state
  void printStatus() {
    Serial.printf("[MB] Coils: [%s] [%s] [%s] [%s]\n",
                  coilState[0] ? "ON" : "OFF",
                  coilState[1] ? "ON" : "OFF",
                  coilState[2] ? "ON" : "OFF",
                  coilState[3] ? "ON" : "OFF");
  }

private:
  uint8_t slaveAddress;
  bool coilState[4];  // Relay 1-4 states
  bool enabled;

  void processModbusFrame(uint8_t* frame, int frameLen) {
    uint8_t funcCode = frame[1];
    
    // CRC check (simplified - check last 2 bytes)
    uint16_t crcRx = (frame[frameLen-2] << 8) | frame[frameLen-1];
    uint16_t crcCalc = calculateCRC(frame, frameLen - 2);
    
    if (crcRx != crcCalc) {
      Serial.printf("[MB] CRC error: expected 0x%04X, got 0x%04X\n", crcCalc, crcRx);
      return;
    }

    uint8_t response[256];
    int responseLen = 0;

    switch (funcCode) {
      case FC_READ_COILS:
        responseLen = handleReadCoils(frame, response);
        break;
      case FC_WRITE_COIL:
        responseLen = handleWriteCoil(frame, response);
        break;
      case FC_WRITE_COILS:
        responseLen = handleWriteCoils(frame, response);
        break;
      case FC_READ_INPUT_REGS:
        responseLen = handleReadInputRegs(frame, response);
        break;
      default:
        Serial.printf("[MB] Unsupported function code: 0x%02X\n", funcCode);
        return;
    }

    if (responseLen > 0) {
      sendModbusFrame(response, responseLen);
    }
  }

  int handleReadCoils(uint8_t* frame, uint8_t* response) {
    // FC 0x01: Read coils (relays 1-4)
    uint16_t addr = (frame[2] << 8) | frame[3];
    uint16_t count = (frame[4] << 8) | frame[5];

    if (addr >= 4 || count > 4 || (addr + count) > 4) {
      return -1;  // Invalid address
    }

    response[0] = frame[0];  // Slave address
    response[1] = frame[1];  // Function code
    response[2] = 1;         // Byte count
    uint8_t coilByte = 0;
    for (int i = 0; i < count; i++) {
      if (coilState[addr + i]) coilByte |= (1 << i);
    }
    response[3] = coilByte;

    uint16_t crc = calculateCRC(response, 4);
    response[4] = crc & 0xFF;
    response[5] = (crc >> 8) & 0xFF;

    Serial.printf("[MB] Read coils: addr=%u count=%u mask=0x%02X\n", addr, count, coilByte);
    return 6;
  }

  int handleWriteCoil(uint8_t* frame, uint8_t* response) {
    // FC 0x05: Write single coil
    uint16_t addr = (frame[2] << 8) | frame[3];
    uint16_t value = (frame[4] << 8) | frame[5];

    if (addr >= 4) return -1;

    bool state = (value == 0xFF00);
    coilState[addr] = state;

    // Echo back the request
    memcpy(response, frame, 6);
    uint16_t crc = calculateCRC(response, 6);
    response[6] = crc & 0xFF;
    response[7] = (crc >> 8) & 0xFF;

    Serial.printf("[MB] Write coil: addr=%u value=%s\n", addr, state ? "ON" : "OFF");
    return 8;
  }

  int handleWriteCoils(uint8_t* frame, uint8_t* response) {
    // FC 0x0F: Write multiple coils
    uint16_t addr = (frame[2] << 8) | frame[3];
    uint16_t count = (frame[4] << 8) | frame[5];
    uint8_t byteCount = frame[6];

    if (addr >= 4 || count > 4 || (addr + count) > 4) return -1;

    for (int i = 0; i < count; i++) {
      uint8_t byte = frame[7 + (i / 8)];
      bool bit = (byte >> (i % 8)) & 1;
      coilState[addr + i] = bit;
    }

    response[0] = frame[0];
    response[1] = frame[1];
    response[2] = frame[2];
    response[3] = frame[3];
    response[4] = frame[4];
    response[5] = frame[5];

    uint16_t crc = calculateCRC(response, 6);
    response[6] = crc & 0xFF;
    response[7] = (crc >> 8) & 0xFF;

    Serial.printf("[MB] Write %u coils starting at addr %u\n", count, addr);
    return 8;
  }

  int handleReadInputRegs(uint8_t* frame, uint8_t* response) {
    // FC 0x04: Read input registers (status, flow count, etc.)
    // For now, just return zeros as placeholder
    response[0] = frame[0];
    response[1] = frame[1];
    response[2] = 4;  // Byte count (2 registers * 2 bytes)
    response[3] = 0x00;
    response[4] = 0x00;
    response[5] = 0x00;
    response[6] = 0x00;

    uint16_t crc = calculateCRC(response, 7);
    response[7] = crc & 0xFF;
    response[8] = (crc >> 8) & 0xFF;

    return 9;
  }

  void sendModbusFrame(uint8_t* frame, int len) {
    digitalWrite(RS485_RD_PIN, HIGH);  // Enable transmit mode
    delayMicroseconds(100);

    Serial1.write(frame, len);
    Serial1.flush();

    delayMicroseconds(100);
    digitalWrite(RS485_RD_PIN, LOW);   // Return to receive mode

    Serial.printf("[MB] Sent frame: %d bytes\n", len);
  }

  uint16_t calculateCRC(uint8_t* frame, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
      crc ^= frame[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 0x0001) {
          crc = (crc >> 1) ^ 0xA001;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc;
  }
};
