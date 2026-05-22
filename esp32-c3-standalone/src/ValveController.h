#pragma once
#include <Arduino.h>

class ValveController {
public:
    ValveController(int pin) : pin(pin), isOpen(false), lastCommandTime(0) {}
    
    void begin() {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        isOpen = false;
    }
    
    void open() {
        digitalWrite(pin, HIGH);
        isOpen = true;
        lastCommandTime = millis();
        Serial.printf("[VALVE] Pin %d OPEN\n", pin);
    }
    
    void close() {
        digitalWrite(pin, LOW);
        isOpen = false;
        lastCommandTime = millis();
        Serial.printf("[VALVE] Pin %d CLOSE\n", pin);
    }
    
    bool getState() const {
        return isOpen;
    }
    
    // Safety: auto-close if no commands for too long
    void checkSafety() {
        if (isOpen && (millis() - lastCommandTime) > VALVE_SAFETY_TIMEOUT_MS) {
            close();
            Serial.printf("[VALVE] Pin %d auto-closed (safety timeout)\n", pin);
        }
    }

private:
    int pin;
    bool isOpen;
    unsigned long lastCommandTime;
};