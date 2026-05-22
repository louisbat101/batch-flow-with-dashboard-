#include "FlowMeter.h"

FlowMeter::FlowMeter(int pin): pin(pin) {
  pulses = 0;
}

void FlowMeter::pulseISR() {
  pulses++;
}

void FlowMeter::setPulsesPerLitre(float ppl) {
  pulsesPerLitre = ppl;
}

float FlowMeter::litres() {
  noInterrupts();
  unsigned long p = pulses;
  interrupts();
  return (float)p / pulsesPerLitre;
}

void FlowMeter::resetLitres() {
  noInterrupts();
  pulses = 0;
  interrupts();
}
