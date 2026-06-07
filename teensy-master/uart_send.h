// ═══════════════════════════════════════════════════════
// UART STATUS TRANSMISSION (to C3 Master)
// ═══════════════════════════════════════════════════════

void sendBoardStatusToC3(int boardAddr) {
  // Format: [START=0xFF] [CMD=0x01] [LEN] [DATA...] [CRC] [END=0xFE]
  // Data: address(1) + status(1) + target(4) + dispensed(4) + valve(1) + pulses(4) = 15 bytes
  
  if (boardAddr < 1 || boardAddr > 4) return;
  
  Board &board = boards[boardAddr - 1];
  uint8_t msg[25];  // Enough for entire message
  
  int idx = 0;
  msg[idx++] = 0xFF;              // START
  msg[idx++] = 0x01;              // CMD: Status Report
  msg[idx++] = 15;                // DATA LENGTH
  msg[idx++] = boardAddr;         // Board address
  msg[idx++] = board.online ? (board.dispensing ? 2 : 1) : 0;  // Status: 0=offline, 1=idle, 2=dispensing
  
  // Target amount (4-byte float)
  float target = board.targetAmount;
  memcpy(&msg[idx], &target, 4);
  idx += 4;
  
  // Dispensed amount (4-byte float)
  float dispensed = board.dispensedAmount;
  memcpy(&msg[idx], &dispensed, 4);
  idx += 4;
  
  msg[idx++] = 0;  // Valve state (placeholder)
  
  // Pulse count (4-byte uint32)
  uint32_t pulses = board.pulseCount;
  memcpy(&msg[idx], &pulses, 4);
  idx += 4;
  
  // Calculate XOR checksum over data bytes
  uint8_t crc = 0;
  for (int i = 2; i < idx; i++) {  // Skip START and CMD
    crc ^= msg[i];
  }
  msg[idx++] = crc;
  msg[idx++] = 0xFE;  // END
  
  // Send via Serial1 to C3 Master
  Serial1.write(msg, idx);
  Serial1.flush();
  
  Serial.printf("[UART→C3] Board %d status sent (%d bytes)\n", boardAddr, idx);
}

void sendAllBoardStatusToC3() {
  // Send status for all boards in sequence
  for (int i = 0; i < boardCount; i++) {
    sendBoardStatusToC3(boards[i].address);
    delayMicroseconds(100);  // Small delay between messages
  }
}
