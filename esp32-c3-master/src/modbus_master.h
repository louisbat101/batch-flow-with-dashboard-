#pragma once
#include <Arduino.h>
#include "config.h"

class ModbusMaster {
private:
  static constexpr uint8_t MODBUS_TIMEOUT_MS = 50;  // Reduced timeout for faster polling
  
  // CRC-16 CCITT
  static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
        if (crc & 1) {
          crc = (crc >> 1) ^ 0xA001;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc;
  }

public:
  static void begin() {
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
    pinMode(RS485_RD_PIN, OUTPUT);
    digitalWrite(RS485_RD_PIN, LOW);  // RX mode
    Serial.println("[Modbus] Master initialized on Serial1");
  }

  // Write single coil (0x05) - for valve control
  // Address = slave address, coil = valve state (true/false)
  static bool writeCoil(uint8_t slaveAddr, uint16_t coilAddr, bool state) {
    uint8_t request[8];
    request[0] = slaveAddr;
    request[1] = MODBUS_WRITE_SINGLE_COIL;
    request[2] = (coilAddr >> 8) & 0xFF;
    request[3] = coilAddr & 0xFF;
    request[4] = state ? 0xFF : 0x00;
    request[5] = 0x00;
    
    uint16_t crc = crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    digitalWrite(RS485_RD_PIN, HIGH);  // TX mode
    delayMicroseconds(500);  // Wait for transceiver to switch to TX
    Serial1.write(request, 8);
    Serial1.flush();
    delayMicroseconds(500);  // Wait for all bytes to transmit
    digitalWrite(RS485_RD_PIN, LOW);   // RX mode
    delayMicroseconds(100);  // Wait for transceiver to switch to RX

    Serial.printf("[Modbus] TX: Addr=%u Fn=0x05 Coil=%u Value=%u\n", slaveAddr, coilAddr, state);

    // Read response (echo + status)
    uint8_t response[8];
    uint32_t startTime = millis();
    size_t idx = 0;

    while (millis() - startTime < MODBUS_TIMEOUT_MS) {
      if (Serial1.available()) {
        response[idx] = Serial1.read();
        idx++;
        if (idx >= 8) break;
      }
    }

    if (idx < 8) {
      Serial.printf("[Modbus] RX timeout (got %u bytes)\n", idx);
      return false;
    }

    uint16_t rxCrc = crc16(response, 6);
    uint16_t declaredCrc = (response[7] << 8) | response[6];
    if (rxCrc != declaredCrc) {
      Serial.println("[Modbus] CRC mismatch!");
      return false;
    }

    Serial.printf("[Modbus] RX: Success\n");
    return true;
  }

  // Read holding registers (0x03) - for status, flow count, etc
  static bool readRegisters(uint8_t slaveAddr, uint16_t startReg, uint16_t count, uint16_t* outRegs) {
    uint8_t request[8];
    request[0] = slaveAddr;
    request[1] = MODBUS_READ_HOLDING_REG;
    request[2] = (startReg >> 8) & 0xFF;
    request[3] = startReg & 0xFF;
    request[4] = (count >> 8) & 0xFF;
    request[5] = count & 0xFF;

    uint16_t crc = crc16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    digitalWrite(RS485_RD_PIN, HIGH);  // TX mode
    delayMicroseconds(500);  // Wait for transceiver to switch to TX
    Serial1.write(request, 8);
    Serial1.flush();
    delayMicroseconds(500);  // Wait for all bytes to transmit
    digitalWrite(RS485_RD_PIN, LOW);   // RX mode
    delayMicroseconds(100);  // Wait for transceiver to switch to RX

    Serial.printf("[Modbus] TX: Addr=%u Fn=0x03 Reg=%u Count=%u\n", slaveAddr, startReg, count);

    // Read response: addr(1) + fn(1) + byteCount(1) + data(N*2) + crc(2)
    uint8_t response[256];
    uint32_t startTime = millis();
    size_t idx = 0;

    while (millis() - startTime < MODBUS_TIMEOUT_MS) {
      if (Serial1.available()) {
        response[idx] = Serial1.read();
        idx++;
        if (idx >= 3 && idx >= (3 + response[2] + 2)) break;
      }
    }

    if (idx < 5) {
      Serial.printf("[Modbus] RX timeout (got %u bytes)\n", idx);
      return false;
    }

    uint16_t rxCrc = crc16(response, idx - 2);
    uint16_t declaredCrc = (response[idx - 1] << 8) | response[idx - 2];
    if (rxCrc != declaredCrc) {
      Serial.println("[Modbus] CRC mismatch!");
      return false;
    }

    // Extract registers
    uint8_t byteCount = response[2];
    for (int i = 0; i < count && i < (byteCount / 2); i++) {
      outRegs[i] = (response[3 + i*2] << 8) | response[3 + i*2 + 1];
    }

    Serial.printf("[Modbus] RX: Success (%u registers)\n", count);
    return true;
  }

  // Convenience: Open valve on slave
  static bool openValve(uint8_t slaveAddr) {
    return writeCoil(slaveAddr, SLAVE_REG_VALVE_CONTROL, true);
  }

  // Convenience: Close valve on slave
  static bool closeValve(uint8_t slaveAddr) {
    return writeCoil(slaveAddr, SLAVE_REG_VALVE_CONTROL, false);
  }

  // Convenience: Get flow count from slave
  static bool getFlowCount(uint8_t slaveAddr, uint16_t& flowCount) {
    uint16_t regs[1];
    if (readRegisters(slaveAddr, SLAVE_REG_FLOW_COUNT, 1, regs)) {
      flowCount = regs[0];
      return true;
    }
    return false;
  }

  // Convenience: Get slave status
  static bool getStatus(uint8_t slaveAddr, uint16_t& status) {
    uint16_t regs[1];
    if (readRegisters(slaveAddr, SLAVE_REG_STATUS, 1, regs)) {
      status = regs[0];
      return true;
    }
    return false;
  }
};
