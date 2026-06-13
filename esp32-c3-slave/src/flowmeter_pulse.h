#pragma once
#include <Arduino.h>

// ── Flowmeter Pulse ISR for C3 Slave ─────────────────────────────
// Attach to flowmeter pulse pin (default GPIO 8)
// Counts pulses and makes them available in the Modbus registers

class FlowmeterPulse {
public:
  FlowmeterPulse(uint8_t pin, volatile uint32_t &pulseCounter)
    : _pin(pin), _counter(pulseCounter) {}

  void begin() {
    pinMode(_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pin), [this]() { this->_counter++; }, FALLING);
    Serial.printf("  ✅ Flowmeter ISR on GPIO %d\n", _pin);
  }

private:
  uint8_t _pin;
  volatile uint32_t &_counter;
};
