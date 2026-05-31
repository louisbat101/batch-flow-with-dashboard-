/*
 * Intelligent Batching Control System
 * Pulse-based batching with adaptive shutoff
 * 
 * Features:
 * - Pulse-count batching instead of time-based
 * - Dynamic shutoff calculation using flow rate
 * - Adaptive learning for batch error correction
 * - Support for manual and intelligent modes
 */

#ifndef BATCHING_CONTROL_H
#define BATCHING_CONTROL_H

#include <Arduino.h>

// Batching modes
enum BatchingMode {
  MODE_MANUAL = 0,      // Fixed offset, user-defined
  MODE_INTELLIGENT = 1  // Dynamic offset calculated from flow rate
};

// Batch state machine
enum BatchState {
  STATE_IDLE = 0,
  STATE_PRIMING = 1,
  STATE_RUNNING = 2,
  STATE_SHUTOFF = 3,
  STATE_DONE = 4,
  STATE_ERROR = 5
};

struct BatchingConfig {
  // Product calibration
  float pulsesPerGallon;    // PPG: pulses per gallon (calibrated)
  float pulsesPerLiter;     // PPL: pulses per liter (calibrated)
  
  // Shutoff control
  BatchingMode shutoffMode;
  float manualOffsetGallons;    // Fixed offset for manual mode
  float valveCloseTimeSecs;     // Time for valve to fully close (seconds)
  
  // Adaptive learning
  bool adaptiveLearning;
  float learningGain;           // 0.0 - 1.0, how aggressively to learn
  float learnCorrection;        // Accumulated correction from batches
};

struct BatchingState {
  // Batch targets
  float targetGallons;
  float targetPulses;
  
  // Current batch progress
  uint32_t pulsesReceived;
  float currentVolume;          // Gallons dispensed so far
  
  // Flow rate calculation
  uint32_t lastPulseTime;       // Milliseconds
  uint32_t pulsesSinceLastCalc;
  float currentFlowRateGPM;     // Gallons per minute
  float currentFlowRatePPS;     // Pulses per second
  
  // Shutoff point
  float dynamicOffset;          // Calculated offset
  float shutoffPoint;           // Target - offset = shutoff point
  
  // State tracking
  BatchState state;
  uint32_t startTime;
  uint32_t lastPulseReceivedTime;
  
  // Error tracking
  float lastBatchError;         // Actual - Target
};

class BatchingController {
private:
  BatchingConfig config;
  BatchingState state;
  
public:
  BatchingController() {
    memset(&config, 0, sizeof(config));
    memset(&state, 0, sizeof(state));
    state.state = STATE_IDLE;
  }
  
  // ═══════════════════════════════════════════════════════
  // CONFIGURATION
  // ═══════════════════════════════════════════════════════
  
  void configureProduct(float ppl, float ppg, float valveCloseTime) {
    config.pulsesPerLiter = ppl;
    config.pulsesPerGallon = ppg;
    config.valveCloseTimeSecs = valveCloseTime;
    config.learnCorrection = 0.0;
  }
  
  void setShutoffMode(BatchingMode mode) {
    config.shutoffMode = mode;
  }
  
  void setManualOffset(float offsetGallons) {
    config.manualOffsetGallons = offsetGallons;
  }
  
  void setAdaptiveLearning(bool enabled, float gain = 0.1) {
    config.adaptiveLearning = enabled;
    config.learningGain = constrain(gain, 0.0, 1.0);
  }
  
  // ═══════════════════════════════════════════════════════
  // BATCHING CONTROL
  // ═══════════════════════════════════════════════════════
  
  void startBatch(float targetGallons) {
    state.targetGallons = targetGallons;
    state.targetPulses = targetGallons * config.pulsesPerGallon;
    
    state.pulsesReceived = 0;
    state.currentVolume = 0.0;
    state.currentFlowRateGPM = 0.0;
    state.currentFlowRatePPS = 0.0;
    state.dynamicOffset = 0.0;
    
    state.state = STATE_PRIMING;
    state.startTime = millis();
    state.lastPulseTime = millis();
    state.pulsesSinceLastCalc = 0;
  }
  
  void stopBatch() {
    state.state = STATE_DONE;
  }
  
  void abortBatch() {
    state.state = STATE_IDLE;
    state.pulsesReceived = 0;
    state.currentVolume = 0.0;
  }
  
  // ═══════════════════════════════════════════════════════
  // PULSE INPUT & FLOW CALCULATION
  // ═══════════════════════════════════════════════════════
  
  void receivePulse() {
    if (state.state == STATE_IDLE) return;
    
    state.pulsesReceived++;
    state.pulsesSinceLastCalc++;
    state.currentVolume = (float)state.pulsesReceived / config.pulsesPerGallon;
    state.lastPulseReceivedTime = millis();
    
    // Update flow rate every 100ms
    uint32_t now = millis();
    uint32_t timeDeltaMs = now - state.lastPulseTime;
    
    if (timeDeltaMs >= 100 && state.pulsesSinceLastCalc > 0) {
      // Flow rate = pulses/sec / pulses/gallon = gallons/sec
      float deltaSeconds = timeDeltaMs / 1000.0;
      float deltaPulses = state.pulsesSinceLastCalc;
      
      state.currentFlowRatePPS = deltaPulses / deltaSeconds;
      state.currentFlowRateGPM = (deltaPulses / config.pulsesPerGallon) / deltaSeconds * 60.0;
      
      state.lastPulseTime = now;
      state.pulsesSinceLastCalc = 0;
      
      updateShutoffPoint();
    }
  }
  
  // ═══════════════════════════════════════════════════════
  // SHUTOFF POINT CALCULATION
  // ═══════════════════════════════════════════════════════
  
  void updateShutoffPoint() {
    if (config.shutoffMode == MODE_MANUAL) {
      calculateManualShutoff();
    } else {
      calculateIntelligentShutoff();
    }
  }
  
  void calculateManualShutoff() {
    // Simple: Target - ManualOffset
    state.shutoffPoint = state.targetPulses - 
      (config.manualOffsetGallons * config.pulsesPerGallon);
    state.dynamicOffset = config.manualOffsetGallons;
  }
  
  void calculateIntelligentShutoff() {
    // Dynamic offset = Flow Rate × Valve Close Time
    // Offset in gallons = (GPM / 60) × valve_close_time_seconds
    
    if (state.currentFlowRateGPM < 0.01) {
      // No flow yet, use manual offset as fallback
      state.dynamicOffset = config.manualOffsetGallons;
    } else {
      float flowRateGPS = state.currentFlowRateGPM / 60.0;  // Convert to gallons per second
      float dynamicOffsetGallons = flowRateGPS * config.valveCloseTimeSecs;
      
      // Add accumulated learning correction
      if (config.adaptiveLearning) {
        dynamicOffsetGallons += config.learnCorrection;
      }
      
      state.dynamicOffset = dynamicOffsetGallons;
    }
    
    // Shutoff point = Target - Dynamic Offset
    state.shutoffPoint = state.targetPulses - 
      (state.dynamicOffset * config.pulsesPerGallon);
  }
  
  // ═══════════════════════════════════════════════════════
  // BATCH MONITORING
  // ═══════════════════════════════════════════════════════
  
  bool shouldShutoff() {
    if (state.state != STATE_RUNNING) return false;
    
    // Close valve when we reach shutoff point
    return state.pulsesReceived >= state.shutoffPoint;
  }
  
  void completeBatch(float actualVolume) {
    // Calculate error and update adaptive correction
    float error = actualVolume - state.targetGallons;
    state.lastBatchError = error;
    
    if (config.adaptiveLearning && state.state == STATE_DONE) {
      // Slowly adjust correction factor
      // If we dispensed too much, increase offset next time
      // If we dispensed too little, decrease offset next time
      config.learnCorrection += (error * config.learningGain);
      
      // Limit learning correction to ±2 gallons
      config.learnCorrection = constrain(config.learnCorrection, -2.0, 2.0);
    }
    
    state.state = STATE_IDLE;
  }
  
  // ═══════════════════════════════════════════════════════
  // STATE QUERIES
  // ═══════════════════════════════════════════════════════
  
  BatchState getState() { return state.state; }
  float getCurrentVolume() { return state.currentVolume; }
  float getTargetVolume() { return state.targetGallons; }
  float getFlowRateGPM() { return state.currentFlowRateGPM; }
  float getShutoffPoint() { return state.shutoffPoint / config.pulsesPerGallon; }
  float getDynamicOffset() { return state.dynamicOffset; }
  float getLearnCorrection() { return config.learnCorrection; }
  float getLastBatchError() { return state.lastBatchError; }
  uint32_t getPulsesReceived() { return state.pulsesReceived; }
  float getProgressPercent() { 
    return (state.currentVolume / state.targetGallons) * 100.0;
  }
};

#endif // BATCHING_CONTROL_H
