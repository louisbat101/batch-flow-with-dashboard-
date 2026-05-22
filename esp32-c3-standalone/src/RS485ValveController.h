#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

class RS485ValveController {
public:
    RS485ValveController(HardwareSerial &serial, int deRePin) : serial(serial), pinDE(deRePin) {
        valve1State = false;
        valve2State = false;
        lastCommandTime = 0;
    }
    
    void begin() {
        pinMode(pinDE, OUTPUT);
        digitalWrite(pinDE, LOW); // Receive mode by default
        serial.begin(RS485_BAUD);
        Serial.println("[RS485] Valve controller initialized");
    }
    
    void openValve(int valveNum) {
        if (valveNum == 1) {
            sendValveCommand(1, true);
            valve1State = true;
        } else if (valveNum == 2) {
            sendValveCommand(2, true);
            valve2State = true;
        }
        lastCommandTime = millis();
        Serial.printf("[RS485] Valve %d OPEN command sent\n", valveNum);
    }
    
    void closeValve(int valveNum) {
        if (valveNum == 1) {
            sendValveCommand(1, false);
            valve1State = false;
        } else if (valveNum == 2) {
            sendValveCommand(2, false);
            valve2State = false;
        }
        lastCommandTime = millis();
        Serial.printf("[RS485] Valve %d CLOSE command sent\n", valveNum);
    }
    
    void closeAllValves() {
        closeValve(1);
        delay(10);
        closeValve(2);
    }
    
    bool getValveState(int valveNum) const {
        if (valveNum == 1) return valve1State;
        if (valveNum == 2) return valve2State;
        return false;
    }
    
    // Safety: auto-close if no commands for too long
    void checkSafety() {
        if ((valve1State || valve2State) && (millis() - lastCommandTime) > VALVE_SAFETY_TIMEOUT_MS) {
            closeAllValves();
            Serial.println("[RS485] Valves auto-closed (safety timeout)");
        }
    }

private:
    HardwareSerial &serial;
    int pinDE;
    bool valve1State, valve2State;
    unsigned long lastCommandTime;

    void sendValveCommand(uint8_t valveAddr, bool open) {
        // Simple Modbus RTU Write Single Coil (0x05) command
        uint8_t frame[8];
        frame[0] = valveAddr;           // Slave address (1 or 2)
        frame[1] = 0x05;                // Function: Write Single Coil
        frame[2] = 0x00;                // Coil address high byte
        frame[3] = 0x00;                // Coil address low byte (0x0000)
        frame[4] = open ? 0xFF : 0x00;  // Value high byte (0xFF00 = ON, 0x0000 = OFF)
        frame[5] = 0x00;                // Value low byte
        
        // Calculate CRC16
        uint16_t crc = calculateCRC16(frame, 6);
        frame[6] = crc & 0xFF;          // CRC low byte
        frame[7] = (crc >> 8) & 0xFF;   // CRC high byte
        
        // Send frame
        digitalWrite(pinDE, HIGH);      // Transmit mode
        delayMicroseconds(50);
        serial.write(frame, 8);
        serial.flush();
        delayMicroseconds(100);
        digitalWrite(pinDE, LOW);       // Back to receive mode
    }
    
    uint16_t calculateCRC16(uint8_t *data, uint8_t len) {
        uint16_t crc = 0xFFFF;
        for (uint8_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
};