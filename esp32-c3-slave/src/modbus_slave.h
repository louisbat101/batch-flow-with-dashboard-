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
      pulseCount(0),
      enabled(false) {}

  void begin() {
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
    pinMode(RS485_RD_PIN, OUTPUT);
    digitalWrite(RS485_RD_PIN, LOW);  // Try LOW = enable both RX and TX
    enabled = true;
    Serial.printf("[MB] Modbus slave %u on RS485 (GPIO%u RX, GPIO%u TX, GPIO%u RD=LOW)\n",
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
    if (!enabled) return;
    
    if (!Serial1.available()) return;

    // Wait a bit for the full frame to arrive (Modbus RTU inter-frame gap)
    delay(5);  // Wait 5ms for complete frame at 9600 baud
    
    // DEBUG: Show we got something
    int avail = Serial1.available();
    Serial.printf("[MB] RX: %d bytes available\n", avail);

    // Read incoming frame
    uint8_t frame[256];
    int frameLen = 0;
    unsigned long timeout = millis() + 50;  // 50ms timeout to read all bytes

    while (Serial1.available() && millis() < timeout && frameLen < 256) {
      frame[frameLen++] = Serial1.read();
      // No delay between bytes - read as fast as possible
    }

    Serial.printf("[MB] RX frame: %d bytes - ", frameLen);
    for (int i = 0; i < frameLen && i < 16; i++) {
      Serial.printf("%02X ", frame[i]);
    }
    Serial.println();

    if (frameLen < 4) {
      Serial.println("[MB] Frame too short, ignoring");
      return;  // Minimum valid frame
    }

    // Check slave address
    if (frame[0] != slaveAddress) {
      Serial.printf("[MB] Wrong address: got %u, expected %u\n", frame[0], slaveAddress);
      return;
    }

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

  // Set pulse count (called from flowmeter)
  void setPulseCount(uint32_t count) {
    pulseCount = count;
  }

  uint32_t getPulseCount() const {
    return pulseCount;
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
  uint32_t pulseCount;  // Flowmeter pulse count
  bool enabled;

  void processModbusFrame(uint8_t* frame, int frameLen) {
    uint8_t funcCode = frame[1];
    
    // CRC check - Modbus CRC is LITTLE-ENDIAN (low byte first)
    uint16_t crcRx = frame[frameLen-1] << 8 | frame[frameLen-2];  // Swap byte order!
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
      case FC_READ_REGS:  // 0x03 - Read holding registers (treat same as input regs)
      case FC_READ_INPUT_REGS:  // 0x04 - Read input registers
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
    // FC 0x03/0x04: Read holding/input registers
    // Register map:
    //   Reg 0: Status (bit 0 = valve open)
    //   Reg 1: Pulse count high word
    //   Reg 2: Pulse count low word
    //   Reg 3: Valve state (1=open, 0=closed)
    
    response[0] = frame[0];  // Slave address
    response[1] = frame[1];  // Function code
    response[2] = 8;  // Byte count (4 registers * 2 bytes)
    
    // Reg 0: Status flags
    uint16_t status = coilState[0] ? 1 : 0;  // Bit 0 = relay 1 (main valve)
    response[3] = (status >> 8) & 0xFF;
    response[4] = status & 0xFF;
    
    // Reg 1-2: Pulse count (32-bit split into 2 registers)
    response[5] = (pulseCount >> 24) & 0xFF;  // High word high byte
    response[6] = (pulseCount >> 16) & 0xFF;  // High word low byte
    response[7] = (pulseCount >> 8) & 0xFF;   // Low word high byte
    response[8] = pulseCount & 0xFF;          // Low word low byte
    
    // Reg 3: Valve state (simplified - just return relay 1 state)
    uint16_t valveState = coilState[0] ? 1 : 0;
    response[9] = (valveState >> 8) & 0xFF;
    response[10] = valveState & 0xFF;

    uint16_t crc = calculateCRC(response, 11);
    response[11] = crc & 0xFF;
    response[12] = (crc >> 8) & 0xFF;

    Serial.printf("[MB] Read regs: status=%u pulses=%lu valve=%u\n", status, pulseCount, valveState);
    return 13;
  }

  void sendModbusFrame(uint8_t* frame, int len) {
    Serial.printf("[MB] Sending %d bytes: ", len);
    for (int i = 0; i < len && i < 16; i++) {
      Serial.printf("%02X ", frame[i]);
    }
    Serial.println();

    // Set RD pin HIGH for transmit mode
    digitalWrite(RS485_RD_PIN, HIGH);
    delayMicroseconds(100);  // Small delay before TX

    Serial1.write(frame, len);
    Serial1.flush();

    delayMicroseconds(100);  // Small delay after TX
    
    // Set RD pin back LOW for receive mode
    digitalWrite(RS485_RD_PIN, LOW);

    Serial.printf("[MB] TX complete\n");
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
