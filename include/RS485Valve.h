#ifndef RS485VALVE_H
#define RS485VALVE_H
#include <Arduino.h>
#include <HardwareSerial.h>

class RS485Valve {
public:
  RS485Valve(HardwareSerial &serial, int deRePin, uint8_t id);
  void begin();
  void open();
  void close();
private:
  HardwareSerial &serial;
  int pinDE;
  uint8_t slaveId;
  void sendModbusWriteSingleCoil(uint16_t coilAddr, bool value);
};

#endif
