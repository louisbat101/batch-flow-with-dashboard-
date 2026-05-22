#include "RS485Valve.h"
#include <ModbusMaster.h>

RS485Valve::RS485Valve(HardwareSerial &serial, int deRePin, uint8_t id): serial(serial), pinDE(deRePin), slaveId(id) {}

void RS485Valve::begin() {
  pinMode(pinDE, OUTPUT);
  digitalWrite(pinDE, LOW);
}

void RS485Valve::open() {
  sendModbusWriteSingleCoil(0x0000, true);
}

void RS485Valve::close() {
  sendModbusWriteSingleCoil(0x0000, false);
}

// Minimal Modbus RTU frame builder for Write Single Coil (0x05)
void RS485Valve::sendModbusWriteSingleCoil(uint16_t coilAddr, bool value) {
  uint8_t frame[8];
  frame[0] = slaveId;
  frame[1] = 0x05; // function
  frame[2] = (coilAddr >> 8) & 0xFF;
  frame[3] = coilAddr & 0xFF;
  frame[4] = value ? 0xFF : 0x00;
  frame[5] = 0x00;
  // CRC16
  uint16_t crc = 0xFFFF;
  for (int i=0;i<6;i++) {
    crc ^= frame[i];
    for (int j=0;j<8;j++) {
      if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
      else crc >>= 1;
    }
  }
  frame[6] = crc & 0xFF;
  frame[7] = (crc >> 8) & 0xFF;

  // Enable transmit
  digitalWrite(pinDE, HIGH);
  delayMicroseconds(50);
  serial.write(frame, 8);
  serial.flush();
  delay(2);
  digitalWrite(pinDE, LOW);
}
