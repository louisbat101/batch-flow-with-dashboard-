#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// TEENSY 4.1 BATCH FLOW MASTER CONTROLLER
// Controls RS-485 slave devices (flowmeters, valves)
// Communicates with ESP32-C3 via UART for web UI display
// ═══════════════════════════════════════════════════════════════════

// ── Configuration ──────────────────────────────────────────────────
#define RS485_BAUD        9600      // RS-485 bus (to slaves)
#define UART_BAUD         115200    // UART to C3 display
#define RS485_TX_PIN      20        // UART5 TX → RS-485 DI
#define RS485_RX_PIN      21        // UART5 RX ← RS-485 RO
#define RS485_DE_PIN      19        // GPIO 19 direction control
#define UART_RX_PIN       1         // Serial1 RX (from C3)
#define UART_TX_PIN       2         // Serial1 TX (to C3)

// Board states
#define BOARD_OFFLINE     0
#define BOARD_IDLE        1
#define BOARD_DISPENSING  2
#define BOARD_DONE        3

// ── Data Structures ───────────────────────────────────────────────
struct Board {
  uint8_t address;          // Modbus address (1-4)
  uint8_t state;            // Current state
  float targetLiters;       // Target dispensing amount
  float dispensedLiters;    // Amount already dispensed
  bool valveOpen;           // Current valve state
  uint32_t pulseCount;      // Flowmeter pulse count
  uint32_t lastPollTime;    // Last successful poll timestamp
  uint8_t failureCount;     // Consecutive poll failures
};

Board boards[4] = {
  {1, BOARD_IDLE, 0, 0, false, 0, 0, 0},
  {2, BOARD_IDLE, 0, 0, false, 0, 0, 0},
  {3, BOARD_IDLE, 0, 0, false, 0, 0, 0},
  {4, BOARD_IDLE, 0, 0, false, 0, 0, 0}
};

// ── CRC16 Calculation ──────────────────────────────────────────────
uint16_t crc16(const uint8_t* data, size_t len) {
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

// ── RS-485 Direction Control ───────────────────────────────────────
void setRS485Mode(boolean transmit) {
  digitalWrite(RS485_DE_PIN, transmit ? HIGH : LOW);
  delayMicroseconds(10);
}

// ── RS-485 Query Board Status ──────────────────────────────────────
bool queryBoardStatus(uint8_t slaveAddr) {
  // Modbus FC04: Read Input Registers (2 registers for flowrate + status)
  uint8_t request[] = {slaveAddr, 0x04, 0x00, 0x00, 0x00, 0x02};
  uint16_t crc = crc16(request, 6);
  uint8_t fullRequest[] = {slaveAddr, 0x04, 0x00, 0x00, 0x00, 0x02, 
                           (uint8_t)(crc & 0xFF), (uint8_t)((crc >> 8) & 0xFF)};
  
  // Send query
  setRS485Mode(true);
  Serial5.write(fullRequest, 8);
  Serial5.flush();
  setRS485Mode(false);
  
  // Wait for response
  uint8_t response[20] = {0};
  uint32_t startTime = millis();
  int bytesReceived = 0;
  
  while (millis() - startTime < 300) {
    if (Serial5.available()) {
      response[bytesReceived++] = Serial5.read();
      if (bytesReceived >= 20) break;
    }
    yield();
  }
  
  // Validate response
  if (bytesReceived < 7) {
    Serial.printf("[RS485] Board %d: No response (got %d bytes)\n", slaveAddr, bytesReceived);
    return false;
  }
  
  if (response[0] != slaveAddr || response[1] != 0x04) {
    Serial.printf("[RS485] Board %d: Invalid header\n", slaveAddr);
    return false;
  }
  
  uint16_t rxCrc = (response[bytesReceived - 1] << 8) | response[bytesReceived - 2];
  uint16_t calcCrc = crc16(response, bytesReceived - 2);
  
  if (rxCrc != calcCrc) {
    Serial.printf("[RS485] Board %d: CRC error\n", slaveAddr);
    return false;
  }
  
  Serial.printf("[RS485] Board %d: OK (received %d bytes)\n", slaveAddr, bytesReceived);
  return true;
}

// ── UART Communication Protocol ────────────────────────────────────

// Message format: [0xFF] [CMD] [LEN] [DATA...] [CHECKSUM] [0xFE]
#define MSG_START  0xFF
#define MSG_END    0xFE
#define CMD_STATUS 0x01    // Teensy → C3: Status report
#define CMD_CONTROL 0x02   // C3 → Teensy: Control command
#define CMD_QUERY  0x03    // C3 → Teensy: Query status
#define CMD_ACK    0x81    // Teensy → C3: Acknowledge

uint8_t calcChecksum(const uint8_t* data, uint8_t len) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < len; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

void sendStatusReport() {
  // Send status for all 4 boards
  for (int i = 0; i < 4; i++) {
    uint8_t data[16];
    uint8_t pos = 0;
    
    data[pos++] = boards[i].address;
    data[pos++] = boards[i].state;
    
    // Convert float to 4 bytes (big endian)
    uint32_t targetBits;
    memcpy(&targetBits, &boards[i].targetLiters, 4);
    data[pos++] = (targetBits >> 24) & 0xFF;
    data[pos++] = (targetBits >> 16) & 0xFF;
    data[pos++] = (targetBits >> 8) & 0xFF;
    data[pos++] = targetBits & 0xFF;
    
    uint32_t dispensedBits;
    memcpy(&dispensedBits, &boards[i].dispensedLiters, 4);
    data[pos++] = (dispensedBits >> 24) & 0xFF;
    data[pos++] = (dispensedBits >> 16) & 0xFF;
    data[pos++] = (dispensedBits >> 8) & 0xFF;
    data[pos++] = dispensedBits & 0xFF;
    
    data[pos++] = boards[i].valveOpen ? 1 : 0;
    data[pos++] = (boards[i].pulseCount >> 24) & 0xFF;
    data[pos++] = (boards[i].pulseCount >> 16) & 0xFF;
    data[pos++] = (boards[i].pulseCount >> 8) & 0xFF;
    data[pos++] = boards[i].pulseCount & 0xFF;
    
    uint8_t checksum = calcChecksum(data, pos);
    
    Serial1.write(MSG_START);
    Serial1.write(CMD_STATUS);
    Serial1.write(pos);
    Serial1.write(data, pos);
    Serial1.write(checksum);
    Serial1.write(MSG_END);
  }
}

void processControlCommand() {
  if (!Serial1.available()) return;
  
  uint8_t start = Serial1.read();
  if (start != MSG_START) return;
  
  if (!Serial1.available()) return;
  uint8_t cmd = Serial1.read();
  
  if (!Serial1.available()) return;
  uint8_t len = Serial1.read();
  
  if (len > 32) return;  // Invalid length
  
  uint8_t data[32];
  uint32_t startTime = millis();
  int read = 0;
  
  while (read < len && millis() - startTime < 100) {
    if (Serial1.available()) {
      data[read++] = Serial1.read();
    }
    yield();
  }
  
  if (read < len) return;  // Timeout
  
  if (!Serial1.available()) return;
  uint8_t checksum = Serial1.read();
  
  if (!Serial1.available()) return;
  uint8_t end = Serial1.read();
  
  if (end != MSG_END) return;
  
  // Verify checksum
  if (checksum != calcChecksum(data, len)) {
    Serial.println("[UART] Checksum error!");
    return;
  }
  
  if (cmd == CMD_CONTROL) {
    // Parse control command: board_addr, action, product_id, target_liters
    uint8_t boardAddr = data[0];
    uint8_t action = data[1];
    uint8_t productId = data[2];
    
    uint32_t targetBits = ((uint32_t)data[3] << 24) | 
                          ((uint32_t)data[4] << 16) |
                          ((uint32_t)data[5] << 8) |
                          (uint32_t)data[6];
    float targetLiters;
    memcpy(&targetLiters, &targetBits, 4);
    
    Serial.printf("[UART] Control: Board %d, action=%d, product=%d, target=%.2f L\n",
                  boardAddr, action, productId, targetLiters);
    
    if (boardAddr >= 1 && boardAddr <= 4) {
      Board& b = boards[boardAddr - 1];
      if (action == 0) {
        // Stop
        b.state = BOARD_IDLE;
        b.valveOpen = false;
      } else if (action == 1) {
        // Start
        b.state = BOARD_DISPENSING;
        b.targetLiters = targetLiters;
        b.dispensedLiters = 0;
        b.valveOpen = true;
      }
    }
  }
}

// ── Setup ──────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n╔══════════════════════════════════════════════════╗");
  Serial.println("║  TEENSY 4.1 BATCH FLOW MASTER CONTROLLER        ║");
  Serial.println("║  RS-485 Slaves: UART5 (Pins 20/21)              ║");
  Serial.println("║  C3 Display: UART1 (Pins 1/2) @ 115200          ║");
  Serial.println("╚══════════════════════════════════════════════════╝");
  
  // Setup RS-485 UART
  pinMode(RS485_DE_PIN, OUTPUT);
  setRS485Mode(false);
  Serial5.begin(RS485_BAUD);
  
  // Setup C3 Display UART
  Serial1.begin(UART_BAUD);
  
  Serial.println("[OK] RS-485 and Display UART initialized\n");
}

// ── Main Loop ──────────────────────────────────────────────────────
void loop() {
  static uint32_t lastPoll = 0;
  static uint32_t lastStatusSend = 0;
  
  // Poll RS-485 boards every 1 second
  if (millis() - lastPoll >= 1000) {
    lastPoll = millis();
    
    Serial.println("[Poll] Querying boards...");
    for (int i = 0; i < 4; i++) {
      if (queryBoardStatus(boards[i].address)) {
        boards[i].state = BOARD_IDLE;
        boards[i].failureCount = 0;
      } else {
        boards[i].failureCount++;
        if (boards[i].failureCount >= 3) {
          boards[i].state = BOARD_OFFLINE;
        }
      }
    }
  }
  
  // Send status to C3 every 500ms
  if (millis() - lastStatusSend >= 500) {
    lastStatusSend = millis();
    sendStatusReport();
  }
  
  // Process commands from C3
  processControlCommand();
  
  yield();
}
