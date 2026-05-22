#pragma once
#include <Arduino.h>

// Single flowmeter with dual hall sensors (A + B outputs)
class DualHallFlowMeter {
public:
    DualHallFlowMeter(int pinA, int pinB) : pinA(pinA), pinB(pinB), pulsesA(0), pulsesB(0), pulsesPerLitre(450.0f) {}
    
    void begin() {
        pinMode(pinA, INPUT_PULLUP);
        pinMode(pinB, INPUT_PULLUP);
        pulsesA = 0;
        pulsesB = 0;
    }
    
    void pulseISR_A() {
        pulsesA++;
    }
    
    void pulseISR_B() {
        pulsesB++;
    }
    
    void setPulsesPerLitre(float ppl) {
        if (ppl > 0) pulsesPerLitre = ppl;
    }
    
    float getPulsesPerLitre() const {
        return pulsesPerLitre;
    }
    
    // Get combined flow reading (A + B sensors for better accuracy)
    float getLitres() const {
        noInterrupts();
        uint32_t totalPulses = pulsesA + pulsesB;
        interrupts();
        return (float)totalPulses / pulsesPerLitre;
    }
    
    // Get individual sensor readings
    uint32_t getPulsesA() const {
        noInterrupts();
        uint32_t p = pulsesA;
        interrupts();
        return p;
    }
    
    uint32_t getPulsesB() const {
        noInterrupts();
        uint32_t p = pulsesB;
        interrupts();
        return p;
    }
    
    uint32_t getTotalPulses() const {
        noInterrupts();
        uint32_t total = pulsesA + pulsesB;
        interrupts();
        return total;
    }
    
    void reset() {
        noInterrupts();
        pulsesA = 0;
        pulsesB = 0;
        interrupts();
    }

private:
    int pinA, pinB;
    volatile uint32_t pulsesA, pulsesB;
    float pulsesPerLitre;
};