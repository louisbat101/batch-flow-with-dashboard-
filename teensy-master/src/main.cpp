#include <Arduino.h>

#define RS485_DE_PIN 11
#define RS485_BAUD 9600

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

void setRS485Mode(boolean transmit) {
  digitalWrite(RS485_DE_PIN, transmit ? HIGH : LOW);
  delayMicroseconds(10);
}

uint8_t queryBoardStatus(uint8_t slaveAddr) {
  uint8_t request[] = {slaveAddr, 0x04, 0x00, 0x00, 0x00, 0x02};
  uint16_t crc = crc16(request, 6);
  uint8_t fullRequest[] = {slaveAddr, 0x04, 0x00, 0x00, 0x00, 0x02, (uint8_t)(crc & 0xFF), (uint8_t)((crc >> 8) & 0xFF)};
  
  setRS485Mode(true);
  Serial2.write(fullRequest, 8);
  Serial2.flush();
  setRS485Mode(false);
  
  uint8_t response[20] = {0};
  uint32_t startTime = millis();
  int bytesReceived = 0;
  
  while (millis() - startTime < 300) {
    if (Serial2.available()) {
      response[bytesReceived++] = Serial2.read();
      if (bytesReceived >= 20) break;
    }
    yield();
  }
  
  if (bytesReceived < 7) {
    Serial.printf("[RS485] Board %d: No response\n", slaveAddr);
    return 0;
  }
  
  if (response[0] != slaveAddr || response[1] != 0x04) {
    Serial.printf("[RS485] Board %d: Bad header\n", slaveAddr);
    return 0;
  }
  
  uint16_t rxCrc = (response[bytesReceived - 1] << 8) | response[bytesReceived - 2];
  uint16_t calcCrc = crc16(response, bytesReceived - 2);
  
  if (rxCrc != calcCrc) {
    Serial.printf("[RS485] Board %d: CRC error\n", slaveAddr);
    return 0;
  }
  
  Serial.printf("[RS485] Board %d OK\n", slaveAddr);
  return 1;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Teensy 4.1 RS-485 Master ===");
  pinMode(RS485_DE_PIN, OUTPUT);
  setRS485Mode(false);
  Serial2.begin(RS485_BAUD);
  Serial.println("Polling boards 1-4 on RS-485...\n");
}

void loop() {
  static uint32_t lastPoll = 0;
  
  if (millis() - lastPoll >= 1000) {
    lastPoll = millis();
    
    for (int i = 1; i <= 4; i++) {
      queryBoardStatus(i);
      yield();
    }
    Serial.println();
  }
  
  yield();
}
