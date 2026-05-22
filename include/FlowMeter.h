#ifndef FLOWMETER_H
#define FLOWMETER_H
#include <Arduino.h>

class FlowMeter {
public:
  FlowMeter(int pin);
  void pulseISR();
  void setPulsesPerLitre(float ppl);
  float litres();
  void resetLitres();
private:
  volatile unsigned long pulses = 0;
  float pulsesPerLitre = 450.0; // default
  int pin;
};

#endif
