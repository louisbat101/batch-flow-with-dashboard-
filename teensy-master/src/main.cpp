#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
// TEENSY 4.1 BATCH FLOW MASTER CONTROLLER
// Controls RS-485 slave devices (flowmeters, valves)
// Sends status over RS-485 for tablet listening
// ═══════════════════════════════════════════════════════════════════

// Firmware version
#define FW_VERSION  "2.1.0"

// Teensy 4.x internal watchdog (no external library needed)
// WDOG1 registers: WDOG1_WCR, WDOG1_WT, WDT_WSR sequences

// ── Configuration ──────────────────────────────────────────────────
#define RS485_BAUD        9600      // RS-485 bus (to slaves + tablet)
#define RS485_TX_PIN      20        // UART5 TX → RS-485 DI
#define RS485_RX_PIN      21        // UART5 RX ← RS-485 RO
#define RS485_DE_PIN      19        // GPIO 19 direction control

// Board states
#define BOARD_OFFLINE     0
#define BOARD_IDLE        1
#define BOARD_DISPENSING  2
#define BOARD_DONE        3

// ── Debug Logging ───────────────────────────────────────
// Set to 0 to silence per-poll/per-status debug messages in production
#define DEBUG_RS485      0
#define DEBUG_CONTROL    1

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
  {1, BOARD_OFFLINE, 0, 0, false, 0, 0, 0},
  {2, BOARD_OFFLINE, 0, 0, false, 0, 0, 0},
  {3, BOARD_OFFLINE, 0, 0, false, 0, 0, 0},
  {4, BOARD_OFFLINE, 0, 0, false, 0, 0, 0}
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

// ── RS-485 Write Single Coil (FC05) ────────────────────────────────
bool writeValveCoil(uint8_t slaveAddr, bool open) {
  uint8_t request[] = {
    slaveAddr,
    0x05,                         // Function Code 05
    0x00, 0x00,                   // Coil address 0x0000 (valve)
    open ? 0xFF : 0x00, 0x00      // 0xFF00 = ON, 0x0000 = OFF
  };
  uint16_t crc = crc16(request, 6);
  uint8_t fullRequest[8];
  memcpy(fullRequest, request, 6);
  fullRequest[6] = crc & 0xFF;
  fullRequest[7] = (crc >> 8) & 0xFF;
  
  setRS485Mode(true);
  Serial5.write(fullRequest, 8);
  Serial5.flush();
  setRS485Mode(false);
  
  // Wait for echo response (should be identical to request)
  uint8_t response[8] = {0};
  uint32_t startTime = millis();
  int bytesReceived = 0;
  
  while (millis() - startTime < 300) {
    if (Serial5.available()) {
      response[bytesReceived++] = Serial5.read();
      if (bytesReceived >= 8) break;
    }
    yield();
  }
  
  if (bytesReceived < 8) {
    Serial.printf("[RS485] Board %d: FC05 no response\n", slaveAddr);
    return false;
  }
  
  // Validate: echo should match request
  if (memcmp(response, fullRequest, 6) != 0) {
    Serial.printf("[RS485] Board %d: FC05 response mismatch\n", slaveAddr);
    return false;
  }
  
  uint16_t rxCrc = (response[7] << 8) | response[6];
  uint16_t calcCrc = crc16(response, 6);
  if (rxCrc != calcCrc) {
    Serial.printf("[RS485] Board %d: FC05 CRC error\n", slaveAddr);
    return false;
  }
  
  Serial.printf("[RS485] Board %d: Valve %s\n", slaveAddr, open ? "OPENED" : "CLOSED");
  return true;
}

// ── RS-485 Write Single Register (FC06) ────────────────────────────
bool writeRegister(uint8_t slaveAddr, uint16_t regAddr, uint16_t value) {
  uint8_t request[] = {
    slaveAddr,
    0x06,                         // Function Code 06
    (uint8_t)((regAddr >> 8) & 0xFF),
    (uint8_t)(regAddr & 0xFF),
    (uint8_t)((value >> 8) & 0xFF),
    (uint8_t)(value & 0xFF)
  };
  uint16_t crc = crc16(request, 6);
  uint8_t fullRequest[8];
  memcpy(fullRequest, request, 6);
  fullRequest[6] = crc & 0xFF;
  fullRequest[7] = (crc >> 8) & 0xFF;
  
  setRS485Mode(true);
  Serial5.write(fullRequest, 8);
  Serial5.flush();
  setRS485Mode(false);
  
  // Wait for echo response
  uint8_t response[8] = {0};
  uint32_t startTime = millis();
  int bytesReceived = 0;
  
  while (millis() - startTime < 300) {
    if (Serial5.available()) {
      response[bytesReceived++] = Serial5.read();
      if (bytesReceived >= 8) break;
    }
    yield();
  }
  
  if (bytesReceived < 8) return false;
  if (memcmp(response, fullRequest, 6) != 0) return false;
  
  uint16_t rxCrc = (response[7] << 8) | response[6];
  uint16_t calcCrc = crc16(response, 6);
  if (rxCrc != calcCrc) return false;
  
  Serial.printf("[RS485] Board %d: Reg 0x%04X = %u\n", slaveAddr, regAddr, value);
  return true;
}

// ── Non-blocking RS-485 Poll State Machine ─────────────────────────
// Instead of blocking 300ms per board, we send one request, return,
// and come back on next loop() iteration to collect the response.

#define POLL_STATE_IDLE      0
#define POLL_STATE_WAITING   1   // waiting for response

uint8_t pollState = POLL_STATE_IDLE;
uint8_t pollBoardIndex = 0;       // which board we're currently polling
uint32_t pollSendTime = 0;        // when we sent the request
uint8_t pollResponse[32];         // response buffer
int pollResponseLen = 0;          // bytes received so far

void startNextPoll() {
  // Send FC04 request for next board
  uint8_t addr = boards[pollBoardIndex].address;
  uint8_t request[] = {addr, 0x04, 0x00, 0x00, 0x00, 0x04};
  uint16_t crc = crc16(request, 6);
  uint8_t fullRequest[] = {addr, 0x04, 0x00, 0x00, 0x00, 0x04,
                           (uint8_t)(crc & 0xFF), (uint8_t)((crc >> 8) & 0xFF)};
  
  setRS485Mode(true);
  Serial5.write(fullRequest, 8);
  Serial5.flush();
  setRS485Mode(false);
  
  pollState = POLL_STATE_WAITING;
  pollSendTime = millis();
  pollResponseLen = 0;
  memset(pollResponse, 0, sizeof(pollResponse));
}

void processResponse() {
  uint8_t addr = boards[pollBoardIndex].address;
  
  // Must receive at least 13 bytes for a valid 4-register FC04 response
  // [addr][FC04][byteCount=8][8 data bytes][CRC_lo][CRC_hi] = 13
  if (pollResponseLen < 13) {
    if (pollResponseLen >= 4 && pollResponse[1] == 0x84) {
      Serial.printf("[RS485] Board %d: FC04 error response\n", addr);
    } else {
      if (DEBUG_RS485) Serial.printf("[RS485] Board %d: No valid response (%d bytes)\n", addr, pollResponseLen);
    }
    boards[pollBoardIndex].failureCount++;
    if (boards[pollBoardIndex].failureCount >= 3) {
      boards[pollBoardIndex].state = BOARD_OFFLINE;
    }
    return;
  }
  
  if (pollResponse[0] != addr || pollResponse[1] != 0x04) {
    Serial.printf("[RS485] Board %d: Invalid header\n", addr);
    boards[pollBoardIndex].failureCount++;
    return;
  }
  
  uint16_t rxCrc = (pollResponse[pollResponseLen - 1] << 8) | pollResponse[pollResponseLen - 2];
  uint16_t calcCrc = crc16(pollResponse, pollResponseLen - 2);
  if (rxCrc != calcCrc) {
    Serial.printf("[RS485] Board %d: CRC error\n", addr);
    boards[pollBoardIndex].failureCount++;
    return;
  }
  
  // ── Parse register data ────────────────────────────────
  uint8_t byteCount = pollResponse[2];
  if (byteCount >= 8) {
    uint16_t regStatus    = (pollResponse[3] << 8) | pollResponse[4];
    uint16_t regFlowrate  = (pollResponse[5] << 8) | pollResponse[6];
    uint16_t regDispensed = (pollResponse[7] << 8) | pollResponse[8];
    uint16_t regTarget    = (pollResponse[9] << 8) | pollResponse[10];
    
    Board& b = boards[pollBoardIndex];
    b.state = (regStatus <= 3) ? (uint8_t)regStatus : BOARD_IDLE;
    b.valveOpen = (regStatus == 2);
    b.dispensedLiters = regDispensed / 10.0f;
    if (regTarget > 0) b.targetLiters = regTarget / 10.0f;
    b.pulseCount = regFlowrate;
    b.lastPollTime = millis();
    b.failureCount = 0;
    
    if (DEBUG_RS485)
      Serial.printf("[RS485] Board %d: OK | state=%u flow=%u disp=%.1fL target=%.1fL\n",
                    addr, regStatus, regFlowrate, b.dispensedLiters, b.targetLiters);
  } else {
    Serial.printf("[RS485] Board %d: Short response (%d bytes)\n", addr, pollResponseLen);
    boards[pollBoardIndex].failureCount++;
  }
  
  // Mark offline after 3 consecutive failures
  if (boards[pollBoardIndex].failureCount >= 3) {
    boards[pollBoardIndex].state = BOARD_OFFLINE;
  }
}

// ── Setup ──────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n╔══════════════════════════════════════════════════╗");
  Serial.println("║  TEENSY 4.1 BATCH FLOW MASTER CONTROLLER        ║");
  Serial.println("║  RS-485 Bus: UART5 (Pins 20/21) @ 9600          ║");
  Serial.println("║  Tablet listens on same RS-485 bus              ║");
  Serial.println("╚══════════════════════════════════════════════════╝");
  Serial.printf("  Firmware: %s\n\n", FW_VERSION);
  
  // Setup RS-485 UART
  pinMode(RS485_DE_PIN, OUTPUT);
  setRS485Mode(false);
  Serial5.begin(RS485_BAUD);
  
  Serial.println("[OK] RS-485 initialized\n");
  
  // ── Watchdog: ~5 second timeout (WDOG1) ──────────────
  __disable_irq();
  WDOG1_WSR = 0x5555;   // unlock sequence
  WDOG1_WSR = 0xAAAA;
  WDOG1_WCR = 0b00011110;  // WDE=1, WDT=1 (SW reset), WT=30 (≈5s @ 24MHz)
  WDOG1_WCR |= (1 << 2);   // set WDA (enable)
  __enable_irq();
  // Feed pattern: write 0x5555 then 0xAAAA to WSR
  #define FEED_WATCHDOG() do { WDOG1_WSR = 0x5555; WDOG1_WSR = 0xAAAA; } while(0)
  Serial.println("[OK] Watchdog enabled (~5s timeout)\n");
  
  // Send version string to USB Serial (for PC debug)
  Serial.printf("[BOOT] Firmware %s\n", FW_VERSION);
}

// ── Main Loop ──────────────────────────────────────────────────────
void loop() {
  static uint32_t lastPoll = 0;
  
  // ── Non-blocking RS-485 polling state machine ──────────
  if (pollState == POLL_STATE_IDLE) {
    // Start next poll cycle every 1 second
    if (millis() - lastPoll >= 1000) {
      lastPoll = millis();
      pollBoardIndex = 0;
      startNextPoll();
    }
  } else if (pollState == POLL_STATE_WAITING) {
    // Collect response bytes
    while (Serial5.available()) {
      if (pollResponseLen < (int)sizeof(pollResponse)) {
        pollResponse[pollResponseLen++] = Serial5.read();
      } else {
        Serial5.read();  // discard overflow
      }
    }
    
    // Timeout or complete?
    if (millis() - pollSendTime > 300) {
      processResponse();
      
      // Move to next board or end cycle
      pollBoardIndex++;
      if (pollBoardIndex < 4) {
        startNextPoll();
      } else {
        pollState = POLL_STATE_IDLE;
      }
    }
  }
  
  // Feed the watchdog
  FEED_WATCHDOG();
  
  yield();
}
