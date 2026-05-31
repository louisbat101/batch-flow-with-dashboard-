#include <Arduino.h>
#include <HardwareSerial.h>

// ═══════════════════════════════════════════════════════
// RS-485 CONFIGURATION (MUST MATCH MASTER)
// ═══════════════════════════════════════════════════════
#define RS485_RXD_PIN     20      // GPIO 20 (RXD) - CORRECT PIN
#define RS485_TXD_PIN     21      // GPIO 21 (TXD) - CORRECT PIN
#define RS485_RD_PIN      9       // GPIO 9 (RD/DE Direction Control) - REQUIRED!
#define RS485_BAUD        9600    // Modbus RTU baud rate
#define SLAVE_ADDRESS     2       // Slave address (1=master, 2-4=slaves)

HardwareSerial rs485(1);

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
struct {
  uint16_t status;        // Register 0: Board status
  uint16_t flowrate;      // Register 1: Current flowrate
  uint16_t dispensed;     // Register 2: Amount dispensed
  uint16_t target;        // Register 3: Target amount
} inputRegisters = {1, 0, 0, 0};  // Status = 1 (online)

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
  
  // Function Code 04: Read Input Registers
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
  inputRegisters.status = 1;      // Online
  inputRegisters.flowrate = 0;    // No flow
  inputRegisters.dispensed = 0;   // Not dispensing
  inputRegisters.target = 0;      // No target
  Serial.println("      ✅ Registers initialized\n");
  
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
  
  // Simulate flowrate changes (for testing)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    inputRegisters.flowrate = random(0, 100);  // Random flowrate 0-100
    Serial.printf("[Sim] Flowrate: %d\n", inputRegisters.flowrate);
  }
  
  delay(1);
}
