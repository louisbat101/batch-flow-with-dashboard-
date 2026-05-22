#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "FlowMeter.h"
#include "RS485ValveController.h"
#include "Database.h"

enum BatchState {
    IDLE,
    BATCHING,
    COMPLETING,
    ERROR
};

class BatchController {
public:
    BatchController(DualHallFlowMeter &flow, RS485ValveController &valves, Database &db) 
        : flowMeter(flow), valveController(valves), database(db) {
        reset();
    }

    void reset() {
        state = IDLE;
        activeValve = 0;
        targetLitres = 0.0f;
        startTime = 0;
        batchStarted = false;
        errorMessage = "";
    }

    bool startBatch(int productId) {
        if (state != IDLE) return false;
        
        Product* product = database.getProduct(productId);
        if (!product) return false;

        // Validate batch parameters
        if (product->targetLitres <= 0 || product->targetLitres > MAX_BATCH_LITRES) {
            errorMessage = "Invalid target litres";
            return false;
        }

        if (product->valve < 1 || product->valve > 2) {
            errorMessage = "Invalid valve selection";
            return false;
        }

        // Setup batch
        activeValve = product->valve;
        targetLitres = product->targetLitres;
        
        // Set flowmeter calibration
        flowMeter.setPulsesPerLitre(product->calibration);
        
        // Reset flowmeter
        flowMeter.reset();
        
        // Start batching
        state = BATCHING;
        startTime = millis();
        batchStarted = true;
        
        // Open the selected valve via RS485
        valveController.openValve(activeValve);
        
        Serial.printf("[BATCH] Started: %s, %.2fL on valve %d\n", 
                     product->name, targetLitres, activeValve);
        
        return true;
    }

    bool startManualBatch(int valveNum, float litres) {
        if (state != IDLE) return false;
        
        // Validate parameters
        if (valveNum < 1 || valveNum > 2) {
            errorMessage = "Invalid valve number";
            return false;
        }
        
        if (litres <= 0 || litres > MAX_BATCH_LITRES) {
            errorMessage = "Invalid target litres";
            return false;
        }

        // Setup manual batch
        activeValve = valveNum;
        targetLitres = litres;
        
        // Reset flowmeter
        flowMeter.reset();
        
        // Start batching
        state = BATCHING;
        startTime = millis();
        batchStarted = true;
        
        // Open the selected valve via RS485
        valveController.openValve(activeValve);
        
        Serial.printf("[BATCH] Manual started: %.2fL on valve %d\n", litres, activeValve);
        
        return true;
    }

    void stopBatch() {
        if (state == IDLE) return;
        
        // Close all valves
        valveController.closeAllValves();
        
        state = IDLE;
        batchStarted = false;
        
        Serial.println("[BATCH] Stopped");
    }

    void update() {
        if (state != BATCHING) return;
        
        // Check for timeout
        if (millis() - startTime > MAX_BATCH_TIME_MS) {
            errorMessage = "Batch timeout";
            state = ERROR;
            stopBatch();
            return;
        }
        
        // Get current flow reading from dual hall sensors
        float currentLitres = flowMeter.getLitres();
        
        // Check if target reached
        if (currentLitres >= targetLitres) {
            state = COMPLETING;
            
            // Close the active valve
            valveController.closeValve(activeValve);
            
            Serial.printf("[BATCH] Complete: %.3fL (target: %.2fL)\n", currentLitres, targetLitres);
            
            // Small delay then return to idle
            delay(100);
            state = IDLE;
            batchStarted = false;
        }
    }

    String getStatusJson() const {
        JsonDocument doc;
        
        doc["state"] = getStateString();
        doc["batching"] = (state == BATCHING);
        doc["active_valve"] = activeValve;
        doc["target_litres"] = targetLitres;
        doc["flow_litres"] = flowMeter.getLitres();
        doc["flow_pulses_a"] = flowMeter.getPulsesA();
        doc["flow_pulses_b"] = flowMeter.getPulsesB();
        doc["flow_total_pulses"] = flowMeter.getTotalPulses();
        doc["valve1_open"] = valveController.getValveState(1);
        doc["valve2_open"] = valveController.getValveState(2);
        doc["uptime_ms"] = millis();
        doc["calibration"] = flowMeter.getPulsesPerLitre();
        
        if (state == BATCHING) {
            doc["batch_time_ms"] = millis() - startTime;
            float currentLitres = flowMeter.getLitres();
            doc["progress_percent"] = (targetLitres > 0) ? (currentLitres / targetLitres * 100.0f) : 0.0f;
        }
        
        if (state == ERROR) {
            doc["error"] = errorMessage;
        }

        String result;
        serializeJson(doc, result);
        return result;
    }

    BatchState getState() const { return state; }
    bool isBatching() const { return state == BATCHING; }

private:
    DualHallFlowMeter &flowMeter;
    RS485ValveController &valveController;
    Database &database;
    
    BatchState state;
    int activeValve;
    float targetLitres;
    unsigned long startTime;
    bool batchStarted;
    String errorMessage;

    const char* getStateString() const {
        switch (state) {
            case IDLE: return "idle";
            case BATCHING: return "batching";
            case COMPLETING: return "completing";
            case ERROR: return "error";
            default: return "unknown";
        }
    }
};