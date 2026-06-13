#include <Arduino.h>
#include <HardwareSerial.h>
#include "slave_config.h"

// ═══════════════════════════════════════════════════════
// RS-485 CONFIGURATION (from slave_config.h)
// ═══════════════════════════════════════════════════════
// RS485_RXD_PIN = 20, RS485_TXD_PIN = 21, RS485_RD_PIN = 9
// RS485_BAUD = 9600

#ifndef SLAVE_ADDRESS
#define SLAVE_ADDRESS     2       // Slave address (1=master, 2-4=slaves)
#endif

HardwareSerial rs485(1);

// ═══════════════════════════════════════════════════════
// VALVE / RELAY / FLOWMETER PINS (from slave_config.h)
// ═══════════════════════════════════════════════════════
// RELAY_CH1_PIN = 5, RELAY_CH2_PIN = 6, RELAY_CH3_PIN = 7, RELAY_CH4_PIN = 10
// FLOWMETER_PIN = 8

#define PULSES_PER_LITER  450.0f  // Default calibration - adjustable via FC06

volatile uint32_t flowPulseCount = 0;

void IRAM_ATTR onFlowPulse() {
  flowPulseCount++;
}

// ═══════════════════════════════════════════════════════
// MODBUS RTU RESPONDER
// ═══════════════════════════════════════════════════════

uint16_t crc16(uint8_t* data, int len) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) crc = (crc >> 1) ^ 0xA001;
      else crc = crc >> 1;
    }
  }
  return crc;
}

// Input registers for this slave (status data)
// Teensy expects dispensed in tenths of liters (e.g. 15 = 1.5 L)
struct {
  uint16_t status;        // Register 0: Board status (0=offline, 1=idle, 2=dispensing)
  uint16_t flowrate;      // Register 1: Current flowrate (pulses/sec)
  uint16_t dispensed;     // Register 2: Amount dispensed (tenths of liters)
  uint16_t target;        // Register 3: Target amount (tenths of liters)
} inputRegisters = {1, 0, 0, 0};  // Status = 1 (idle/online)

void handleModbusRequest(uint8_t* request, int len) {
  if (len < 8) {
    Serial.println("[Modbus] Invalid request length");
    return;
  }
  
  uint8_t slaveAddr = request[0];
  uint8_t funcCode = request[1];
  
  // Only respond if this request is for us
  if (slaveAddr != SLAVE_ADDRESS) {
    return;  // Not for us, ignore
  }
  
  // Validate CRC
  uint16_t reqCrc = crc16(request, len - 2);
  uint16_t recvCrc = (request[len-1] << 8) | request[len-2];
  
  if (reqCrc != recvCrc) {
    Serial.printf("[Modbus] CRC error on request\n");
    return;
  }
  
  Serial.printf("[Modbus] Request from master: FC=%d\n", funcCode);
  
  // ── Function Code 05: Write Single Coil (valve control) ──
  if (funcCode == 0x05) {
    uint16_t coilAddr = (request[2] << 8) | request[3];
    bool coilValue = (request[4] == 0xFF);  // 0xFF00 = ON, 0x0000 = OFF
    
    Serial.printf("[Modbus] FC05: coil=0x%04X value=%d\n", coilAddr, coilValue);
    
    // Relay channels: if coilAddr matches a relay, toggle it
    switch (coilAddr) {
      case 0x0000:  // Valve/Relay CH1
        digitalWrite(RELAY_CH1_PIN, coilValue ? HIGH : LOW);
        inputRegisters.status = coilValue ? 2 : 1;  // 2=dispensing, 1=idle
        break;
      case 0x0001:  // Relay CH2
        digitalWrite(RELAY_CH2_PIN, coilValue ? HIGH : LOW);
        break;
      case 0x0002:  // Relay CH3
        digitalWrite(RELAY_CH3_PIN, coilValue ? HIGH : LOW);
        break;
      case 0x0003:  // Relay CH4
        digitalWrite(RELAY_CH4_PIN, coilValue ? HIGH : LOW);
        break;
      default:
        // Illegal coil address
        uint8_t errResp[5];
        errResp[0] = slaveAddr;
        errResp[1] = funcCode | 0x80;
        errResp[2] = 0x02;  // illegal data address
        uint16_t ecrc = crc16(errResp, 3);
        errResp[3] = ecrc & 0xFF;
        errResp[4] = (ecrc >> 8) & 0xFF;
        digitalWrite(RS485_RD_PIN, HIGH);
        delay(2);
        rs485.write(errResp, 5);
        rs485.flush();
        digitalWrite(RS485_RD_PIN, LOW);
        return;
    }
    
    // Echo request back as success response (standard Modbus)
    digitalWrite(RS485_RD_PIN, HIGH);
    delay(2);
    rs485.write(request, 8);  // Echo same 8 bytes
    rs485.flush();
    digitalWrite(RS485_RD_PIN, LOW);
    Serial.printf("[Modbus] ✅ FC05 done: coil=0x%04X %s\n", coilAddr, coilValue ? "ON" : "OFF");
    return;
  }
  
  // ── Function Code 06: Write Single Register ──
  if (funcCode == 0x06) {
    uint16_t regAddr = (request[2] << 8) | request[3];
    uint16_t regValue = (request[4] << 8) | request[5];
    
    Serial.printf("[Modbus] FC06: reg=0x%04X val=0x%04X (%u)\n", regAddr, regValue, regValue);
    
    switch (regAddr) {
      case 2:  // Register 2: set dispensed (for reset)
        inputRegisters.dispensed = regValue;
        break;
      case 3:  // Register 3: set target
        inputRegisters.target = regValue;
        break;
      default:
        uint8_t errResp[5];
        errResp[0] = slaveAddr;
        errResp[1] = funcCode | 0x80;
        errResp[2] = 0x02;
        uint16_t ecrc = crc16(errResp, 3);
        errResp[3] = ecrc & 0xFF;
        errResp[4] = (ecrc >> 8) & 0xFF;
        digitalWrite(RS485_RD_PIN, HIGH);
        delay(2);
        rs485.write(errResp, 5);
        rs485.flush();
        digitalWrite(RS485_RD_PIN, LOW);
        return;
    }
    
    // Echo with written value
    digitalWrite(RS485_RD_PIN, HIGH);
    delay(2);
    rs485.write(request, 8);
    rs485.flush();
    digitalWrite(RS485_RD_PIN, LOW);
    Serial.printf("[Modbus] ✅ FC06: reg 0x%04X = %u\n", regAddr, regValue);
    return;
  }
  
  // ── Function Code 04: Read Input Registers ──
  if (funcCode == 0x04) {
    uint16_t startAddr = (request[2] << 8) | request[3];
    uint16_t quantity = (request[4] << 8) | request[5];
    
    if (startAddr + quantity > 4) {
      // Error response - illegal data address
      uint8_t response[8];
      response[0] = slaveAddr;
      response[1] = funcCode | 0x80;  // Error bit set
      response[2] = 0x02;             // Exception code: illegal data address
      
      uint16_t crc = crc16(response, 3);
      response[3] = crc & 0xFF;
      response[4] = (crc >> 8) & 0xFF;
      
      digitalWrite(RS485_RD_PIN, HIGH);  // TX mode
      delay(50);
      rs485.write(response, 5);
      rs485.flush();
      digitalWrite(RS485_RD_PIN, LOW);   // RX mode
      return;
    }
    
    // Build response
    uint8_t response[32];
    response[0] = slaveAddr;
    response[1] = funcCode;
    response[2] = quantity * 2;  // Byte count
    
    // Copy register values
    for (int i = 0; i < quantity; i++) {
      uint16_t regValue = 0;
      switch (startAddr + i) {
        case 0: regValue = inputRegisters.status; break;
        case 1: regValue = inputRegisters.flowrate; break;
        case 2: regValue = inputRegisters.dispensed; break;
        case 3: regValue = inputRegisters.target; break;
      }
      response[3 + (i*2)] = (regValue >> 8) & 0xFF;
      response[4 + (i*2)] = regValue & 0xFF;
    }
    
    // Calculate CRC
    int respLen = 3 + (quantity * 2);
    uint16_t crc = crc16(response, respLen);
    response[respLen] = crc & 0xFF;
    response[respLen+1] = (crc >> 8) & 0xFF;
    
    // Send response
    digitalWrite(RS485_RD_PIN, HIGH);  // TX mode
    delay(50);
    rs485.write(response, respLen + 2);
    rs485.flush();
    delay(50);
    digitalWrite(RS485_RD_PIN, LOW);   // RX mode
    
    Serial.printf("[Modbus] ✅ Response sent (regs %d-%d)\n", startAddr, startAddr + quantity - 1);
  }
}

void rs485Loop() {
  static uint8_t buffer[256];
  static int bufferPos = 0;
  static unsigned long lastByte = 0;
  
  while (rs485.available()) {
    uint8_t byte = rs485.read();
    buffer[bufferPos++] = byte;
    lastByte = millis();
    
    if (bufferPos >= 256) {
      bufferPos = 0;  // Overflow protection
    }
  }
  
  // If we have data and it's been quiet for 100ms, process the message
  if (bufferPos > 0 && (millis() - lastByte) > 100) {
    Serial.printf("[RS485] Received %d bytes: ", bufferPos);
    for (int i = 0; i < bufferPos; i++) {
      Serial.printf("%02X ", buffer[i]);
    }
    Serial.println();
    handleModbusRequest(buffer, bufferPos);
    bufferPos = 0;
  }
}

// ═══════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n════════════════════════════════════════════════════");
  Serial.println("     BATCH FLOW SLAVE #" + String(SLAVE_ADDRESS) + " - STARTUP");
  Serial.println("════════════════════════════════════════════════════\n");
  
  // Initialize RS-485
  Serial.println("[1/3] Initializing RS-485...");
  
  pinMode(RS485_RD_PIN, OUTPUT);
  digitalWrite(RS485_RD_PIN, LOW);  // Start in RX mode
  
  rs485.begin(RS485_BAUD, SERIAL_8N1, RS485_RXD_PIN, RS485_TXD_PIN);
  delay(100);
  
  Serial.printf("      Slave Address: %d\n", SLAVE_ADDRESS);
  Serial.printf("      RXD Pin: GPIO %d\n", RS485_RXD_PIN);
  Serial.printf("      TXD Pin: GPIO %d\n", RS485_TXD_PIN);
  Serial.printf("      RD Pin:  GPIO %d\n", RS485_RD_PIN);
  Serial.printf("      Baud:    %d\n", RS485_BAUD);
  Serial.println("      ✅ RS-485 ready\n");
  
  Serial.println("[2/3] Initializing registers...");
  inputRegisters.status = 1;      // Online/idle
  inputRegisters.flowrate = 0;    // No flow
  inputRegisters.dispensed = 0;   // Nothing dispensed
  inputRegisters.target = 0;      // No target
  Serial.println("      ✅ Registers initialized\n");

  // ── Relay/Valve outputs ──────────────────────────────
  Serial.println("[2b/3] Initializing relay/valve outputs...");
  pinMode(RELAY_CH1_PIN, OUTPUT); digitalWrite(RELAY_CH1_PIN, LOW);
  pinMode(RELAY_CH2_PIN, OUTPUT); digitalWrite(RELAY_CH2_PIN, LOW);
  pinMode(RELAY_CH3_PIN, OUTPUT); digitalWrite(RELAY_CH3_PIN, LOW);
  pinMode(RELAY_CH4_PIN, OUTPUT); digitalWrite(RELAY_CH4_PIN, LOW);
  Serial.println("      ✅ Relay pins initialized (all OFF)\n");
  
  // ── Flowmeter pulse ISR ──────────────────────────────
  Serial.println("[2c/3] Initializing flowmeter pulse input...");
  pinMode(FLOWMETER_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOWMETER_PIN), onFlowPulse, FALLING);
  Serial.printf("      ✅ Flowmeter ISR on GPIO %d\n", FLOWMETER_PIN);
  Serial.printf("      ✅ Calibration: %.0f pulses/liter\n", PULSES_PER_LITER);
  
  Serial.println("[3/3] Entering main loop...");
  Serial.println("      Listening for Modbus requests...\n");
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("        ✅ SLAVE #" + String(SLAVE_ADDRESS) + " READY");
  Serial.println("════════════════════════════════════════════════════\n");
}

void loop() {
  // Handle incoming RS-485 Modbus requests
  rs485Loop();
  
  // Report listening status periodically
  static unsigned long lastReport = 0;
  if (millis() - lastReport > 10000) {
    lastReport = millis();
    Serial.printf("[Status] Still listening on address %d... (RS485 available: %d bytes)\n", 
                  SLAVE_ADDRESS, rs485.available());
  }
  
  // ── Update flowrate/dispensed from real pulse count ──
  static unsigned long lastFlowUpdate = 0;
  
  if (millis() - lastFlowUpdate > 1000) {  // Every 1 second
    lastFlowUpdate = millis();
    
    // Atomically read and reset pulse count
    noInterrupts();
    uint32_t deltaPulses = flowPulseCount;
    flowPulseCount = 0;
    interrupts();
    
    inputRegisters.flowrate = deltaPulses;  // pulses per second
    
    // Convert pulses to tenths of liters (matching Teensy's dispensedLiters = regDispensed / 10.0f)
    if (deltaPulses > 0 && PULSES_PER_LITER > 0) {
      // Accumulate float liters internally, store as tenths in register
      static float dispensedLiters = 0.0f;
      dispensedLiters += (float)deltaPulses / PULSES_PER_LITER;
      inputRegisters.dispensed = (uint16_t)(dispensedLiters * 10.0f);
    }
    
    if (deltaPulses > 0) {
      Serial.printf("[Flow] pulses/sec=%u dispensed=%.1fL (reg=%u)\n", 
                    deltaPulses, inputRegisters.dispensed / 10.0f, inputRegisters.dispensed);
    }
  }
  
  delay(1);
}
