#pragma once
#include <Arduino.h>
#include <ModbusMaster.h>
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════
// ModbusMaster Library Wrapper
// Uses the proven ModbusMaster library for reliable RS-485 communication
// ═══════════════════════════════════════════════════════════════════════

class ModbusMasterLib {
private:
  static ModbusMaster node;
  static uint8_t currentSlaveAddr;
  
  // RS-485 transceiver control
  static void preTransmission() {
    digitalWrite(RS485_RD_PIN, HIGH);  // Enable transmit
  }
  
  static void postTransmission() {
    digitalWrite(RS485_RD_PIN, LOW);   // Enable receive
  }

public:
  static void begin() {
    // Initialize Serial1 for RS-485
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
    
    // Setup RS-485 direction control pin
    pinMode(RS485_RD_PIN, OUTPUT);
    digitalWrite(RS485_RD_PIN, LOW);  // Start in receive mode
    
    // Configure ModbusMaster library
    node.begin(1, Serial1);  // Start with slave address 1
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    Serial.println("[Modbus] Library-based master initialized");
    Serial.printf("[Modbus] RS-485: RX=GPIO%d TX=GPIO%d RD=GPIO%d Baud=%d\n", 
                  RS485_RXD_PIN, RS485_TXD_PIN, RS485_RD_PIN, RS485_BAUD);
  }
  
  // Read 4 holding registers from slave (status, pulses_high, pulses_low, valve)
  static bool readRegisters(uint8_t slaveAddr, uint16_t startReg, uint16_t count, uint16_t* outRegs) {
    if (slaveAddr != currentSlaveAddr) {
      node.begin(slaveAddr, Serial1);
      currentSlaveAddr = slaveAddr;
    }
    
    Serial.printf("[Modbus] Reading %u regs from slave %u starting at reg %u...\n", 
                  count, slaveAddr, startReg);
    
    uint8_t result = node.readHoldingRegisters(startReg, count);
    
    if (result == node.ku8MBSuccess) {
      Serial.printf("[Modbus] ✓ SUCCESS! Got %u registers\n", count);
      for (uint16_t i = 0; i < count; i++) {
        outRegs[i] = node.getResponseBuffer(i);
        Serial.printf("  Reg[%u] = 0x%04X (%u)\n", i, outRegs[i], outRegs[i]);
      }
      return true;
    } else {
      Serial.printf("[Modbus] ✗ FAILED! Error code: 0x%02X\n", result);
      if (result == node.ku8MBIllegalFunction) Serial.println("  Error: Illegal Function");
      else if (result == node.ku8MBIllegalDataAddress) Serial.println("  Error: Illegal Data Address");
      else if (result == node.ku8MBIllegalDataValue) Serial.println("  Error: Illegal Data Value");
      else if (result == node.ku8MBSlaveDeviceFailure) Serial.println("  Error: Slave Device Failure");
      else if (result == node.ku8MBInvalidSlaveID) Serial.println("  Error: Invalid Slave ID");
      else if (result == node.ku8MBInvalidFunction) Serial.println("  Error: Invalid Function");
      else if (result == node.ku8MBResponseTimedOut) Serial.println("  Error: Response Timed Out");
      else if (result == node.ku8MBInvalidCRC) Serial.println("  Error: Invalid CRC");
      return false;
    }
  }
  
  // Write single coil (for valve control)
  static bool writeCoil(uint8_t slaveAddr, uint16_t coilAddr, bool state) {
    if (slaveAddr != currentSlaveAddr) {
      node.begin(slaveAddr, Serial1);
      currentSlaveAddr = slaveAddr;
    }
    
    Serial.printf("[Modbus] Writing coil %u on slave %u to %s...\n", 
                  coilAddr, slaveAddr, state ? "ON" : "OFF");
    
    uint8_t result = node.writeSingleCoil(coilAddr, state ? 0xFF00 : 0x0000);
    
    if (result == node.ku8MBSuccess) {
      Serial.println("[Modbus] ✓ Coil write SUCCESS!");
      return true;
    } else {
      Serial.printf("[Modbus] ✗ Coil write FAILED! Error: 0x%02X\n", result);
      return false;
    }
  }
};

// Static member initialization
ModbusMaster ModbusMasterLib::node;
uint8_t ModbusMasterLib::currentSlaveAddr = 0;
