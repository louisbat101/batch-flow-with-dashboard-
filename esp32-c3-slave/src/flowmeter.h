#pragma once

#include <Arduino.h>
#include "slave_config.h"

// ═══════════════════════════════════════════════════════════════════════
// Flowmeter - ISR-based Pulse Counter
// Counts pulses from a flowmeter to calculate dispensed volume
// ═══════════════════════════════════════════════════════════════════════

class FlowMeter {
private:
  volatile uint32_t pulseCount;
  volatile unsigned long lastPulseTime;
  uint32_t pulsesPerLiter;  // Configured from product setup
  float dispensedVolume;    // Liters
  bool flowing;
  unsigned long noFlowTimeout; // Milliseconds before marking as "no flow"
  unsigned long lastFlowTime;

public:
  FlowMeter() : pulseCount(0), lastPulseTime(0), pulsesPerLiter(1000),
                dispensedVolume(0.0f), flowing(false), noFlowTimeout(5000),
                lastFlowTime(0) {}

  void begin() {
    pinMode(FLOWMETER_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOWMETER_PIN), 
                    []() { flowmeterISR(); }, 
                    FALLING);
    Serial.println("[FM] Flowmeter initialized on GPIO " + String(FLOWMETER_PIN));
    Serial.printf("[FM] Default PPL: %u\n", pulsesPerLiter);
  }

  // ISR handler (static, so it can be called from attachInterrupt)
  static void flowmeterISR() {
    extern FlowMeter* flowmeterInstance;
    if (!flowmeterInstance) return;
    
    unsigned long now = micros();
    // Simple debounce: ignore pulses within 1ms of last pulse
    if (now - flowmeterInstance->lastPulseTime < 1000) {
      return;
    }
    
    flowmeterInstance->pulseCount++;
    flowmeterInstance->lastPulseTime = now;
    flowmeterInstance->lastFlowTime = millis();
    flowmeterInstance->flowing = true;
  }

  // Set pulses per liter (from product configuration)
  void setPulsesPerLiter(uint32_t ppl) {
    pulsesPerLiter = ppl;
    Serial.printf("[FM] PPL updated to %u\n", ppl);
  }

  // Get current pulse count
  uint32_t getPulseCount() const {
    return pulseCount;
  }

  // Calculate dispensed volume (liters)
  float getDispensedVolume() const {
    if (pulsesPerLiter == 0) return 0.0f;
    return (float)pulseCount / (float)pulsesPerLiter;
  }

  // Reset pulse counter (call when starting a new dispense job)
  void reset() {
    noInterrupts();
    pulseCount = 0;
    dispensedVolume = 0.0f;
    lastFlowTime = millis();
    interrupts();
    Serial.println("[FM] Pulse counter reset");
  }

  // Check for no-flow condition
  bool isNoFlow() {
    unsigned long now = millis();
    if (flowing && (now - lastFlowTime) > noFlowTimeout) {
      flowing = false;
      Serial.println("[FM] NO FLOW DETECTED");
      return true;
    }
    return false;
  }

  // Update state (call in loop)
  void update() {
    dispensedVolume = getDispensedVolume();
  }

  // Get formatted volume string
  String getVolumeString() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f L", dispensedVolume);
    return String(buf);
  }

  // Debug: print current state
  void printStatus() {
    Serial.printf("[FM] Pulses: %u | Volume: %.2f L | Flowing: %s\n",
                  pulseCount, dispensedVolume, flowing ? "YES" : "NO");
  }
};

// Forward declaration for ISR - will be defined in main.cpp
extern FlowMeter* flowmeterInstance;
