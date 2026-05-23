#pragma once
#include <Arduino.h>
#include "config.h"
#include "database.h"
#include "modbus_master.h"

enum DispenseState {
  STATE_IDLE,           // Waiting for load
  STATE_LOAD_READY,     // Load prepared, waiting to START
  STATE_RUNNING,        // Dispensing in progress
  STATE_PAUSED,         // Dispensing paused
  STATE_COMPLETE,       // All stations done
  STATE_ERROR           // Error occurred
};

struct StationStatus {
  uint8_t slaveAddr;
  uint16_t productId;
  float targetAmount;
  float dispensedAmount;
  uint16_t flowCount;
  uint8_t status;       // 0=idle, 1=dispensing, 2=done, 3=error
  uint32_t startTimeMs;
  bool valveOpen;
};

class BatchController {
private:
  static DispenseState currentState;
  static StationStatus stations[10];  // Up to 10 stations
  static uint8_t numStations;
  static uint8_t currentStationIdx;
  static uint32_t runStartTime;

public:
  // Initialize new load
  static void initLoad() {
    // Clear stations
    memset(stations, 0, sizeof(stations));
    numStations = 0;
    currentState = STATE_LOAD_READY;
    Serial.println("[Batch] Load initialized");
  }

  // Add station to current load
  static void addStation(uint8_t slaveAddr, uint16_t productId, float targetAmount, uint8_t unit) {
    if (numStations >= 10) {
      Serial.println("[Batch] ERROR: Max 10 stations");
      return;
    }
    stations[numStations].slaveAddr = slaveAddr;
    stations[numStations].productId = productId;
    stations[numStations].targetAmount = targetAmount;
    stations[numStations].dispensedAmount = 0;
    stations[numStations].status = 0;
    stations[numStations].valveOpen = false;
    numStations++;
    Serial.printf("[Batch] Station added: Addr=%u Product=%u Amount=%.2f\n", slaveAddr, productId, targetAmount);
  }

  // Start dispensing sequence
  static void startDispensing() {
    if (currentState != STATE_LOAD_READY) {
      Serial.println("[Batch] ERROR: Load not ready");
      return;
    }
    currentState = STATE_RUNNING;
    currentStationIdx = 0;
    runStartTime = millis();
    Serial.println("[Batch] Dispensing started");
  }

  // Stop dispensing (close all valves)
  static void stopDispensing() {
    for (int i = 0; i < numStations; i++) {
      ModbusMaster::closeValve(stations[i].slaveAddr);
      stations[i].valveOpen = false;
    }
    currentState = STATE_IDLE;
    Serial.println("[Batch] Dispensing stopped");
  }

  // Pause dispensing
  static void pauseDispensing() {
    for (int i = 0; i < numStations; i++) {
      ModbusMaster::closeValve(stations[i].slaveAddr);
      stations[i].valveOpen = false;
    }
    currentState = STATE_PAUSED;
    Serial.println("[Batch] Dispensing paused");
  }

  // Resume from pause
  static void resumeDispensing() {
    currentState = STATE_RUNNING;
    Serial.println("[Batch] Dispensing resumed");
  }

  // Update (called in loop) - handle dispensing logic
  static void update() {
    if (currentState != STATE_RUNNING) return;

    // For each station: check flow, compare to target
    bool allDone = true;
    for (int i = 0; i < numStations; i++) {
      StationStatus& st = stations[i];
      
      // Get current flow count from slave
      uint16_t flowCount = 0;
      if (ModbusMaster::getFlowCount(st.slaveAddr, flowCount)) {
        // Convert pulses to volume (depends on product PPL/PPG)
        Product prod;
        if (Database::getProduct(st.productId, prod)) {
          float pulsesPerUnit = (st.productId == 0) ? prod.pulsesPerLiter : prod.pulsesPerGallon;
          st.dispensedAmount = flowCount / pulsesPerUnit;
        }
      }

      // Check if station is done
      if (st.dispensedAmount >= st.targetAmount) {
        if (st.valveOpen) {
          // Close valve
          ModbusMaster::closeValve(st.slaveAddr);
          st.valveOpen = false;
          st.status = 2;  // done
          Serial.printf("[Batch] Station %u DONE (%.2f / %.2f)\n", st.slaveAddr, st.dispensedAmount, st.targetAmount);
        }
      } else {
        // Still dispensing
        if (!st.valveOpen) {
          ModbusMaster::openValve(st.slaveAddr);
          st.valveOpen = true;
          st.status = 1;  // dispensing
          st.startTimeMs = millis();
        }
        allDone = false;
      }
    }

    if (allDone) {
      currentState = STATE_COMPLETE;
      Serial.println("[Batch] All stations complete!");
    }
  }

  // Get current status as JSON
  static String getStatusJSON() {
    String json = "{\"state\":";
    json += String(currentState);
    json += ",\"stations\":[";

    for (int i = 0; i < numStations; i++) {
      if (i > 0) json += ",";
      json += "{\"addr\":";
      json += String(stations[i].slaveAddr);
      json += ",\"target\":";
      json += String(stations[i].targetAmount, 2);
      json += ",\"dispensed\":";
      json += String(stations[i].dispensedAmount, 2);
      json += ",\"status\":";
      json += String(stations[i].status);
      json += "}";
    }
    json += "]}";
    return json;
  }

  // Getters
  static DispenseState getState() { return currentState; }
  static uint8_t getNumStations() { return numStations; }
  static StationStatus* getStations() { return stations; }
};

// Static member initialization
DispenseState BatchController::currentState = STATE_IDLE;
StationStatus BatchController::stations[10];
uint8_t BatchController::numStations = 0;
uint8_t BatchController::currentStationIdx = 0;
uint32_t BatchController::runStartTime = 0;
