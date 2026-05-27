#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════════════════════
// WiFi Master-Slave Communication (HTTP-based alternative to RS-485)
// Master sends commands to Slave via HTTP requests
// ═══════════════════════════════════════════════════════════════════════

class WiFiMaster {
private:
  static constexpr const char* SLAVE_IP = "192.168.5.1";
  static constexpr uint16_t SLAVE_PORT = 80;
  static constexpr uint32_t TIMEOUT_MS = 2000;
  
public:
  // Get slave status (valve state, pulse count, etc)
  static bool getSlaveStatus(JsonDocument& doc) {
    HTTPClient http;
    String url = String("http://") + SLAVE_IP + ":" + SLAVE_PORT + "/api/status";
    
    http.setTimeout(TIMEOUT_MS);
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      DeserializationError error = deserializeJson(doc, http.getStream());
      http.end();
      return !error;
    }
    
    http.end();
    return false;
  }
  
  // Set relay state (valve control)
  static bool setRelayState(uint8_t relayNum, bool state) {
    HTTPClient http;
    String url = String("http://") + SLAVE_IP + ":" + SLAVE_PORT + "/api/relay";
    
    DynamicJsonDocument payload(256);
    payload["relay"] = relayNum;
    payload["state"] = state;
    
    String body;
    serializeJson(payload, body);
    
    http.setTimeout(TIMEOUT_MS);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(body);
    
    bool success = (httpCode == 200);
    http.end();
    return success;
  }
  
  // Query slave valve state
  static bool getRelayState(uint8_t relayNum, bool& outState) {
    HTTPClient http;
    String url = String("http://") + SLAVE_IP + ":" + SLAVE_PORT + 
                 "/api/relay?relay=" + String(relayNum);
    
    http.setTimeout(TIMEOUT_MS);
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, http.getStream());
      http.end();
      
      if (!error && doc.containsKey("state")) {
        outState = doc["state"];
        return true;
      }
    }
    
    http.end();
    return false;
  }
};

// ═══════════════════════════════════════════════════════════════════════
// WiFi Slave - HTTP endpoints for master to control
// ═══════════════════════════════════════════════════════════════════════

class WiFiSlave {
public:
  // Status endpoint - master queries this to get slave state
  static void handleStatus(WebServer& server, const bool* relayState, size_t numRelays, 
                          uint32_t pulseCount, float flowRate) {
    DynamicJsonDocument doc(512);
    
    doc["online"] = true;
    doc["slaveAddr"] = 1;
    
    JsonArray relays = doc.createNestedArray("relays");
    for (size_t i = 0; i < numRelays; i++) {
      relays.add(relayState[i]);
    }
    
    doc["pulseCount"] = pulseCount;
    doc["flowRate"] = flowRate;
    doc["timestamp"] = millis();
    
    String response;
    serializeJson(doc, response);
    
    server.sendHeader("Content-Type", "application/json");
    server.send(200, "application/json", response);
  }
  
  // Relay control endpoint - master sends this to control relays
  static void handleRelayControl(WebServer& server, bool* relayState, size_t numRelays,
                                 const std::function<void(uint8_t, bool)>& setRelayCallback) {
    if (server.method() == HTTP_POST && server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      DeserializationError error = deserializeJson(doc, server.arg("plain"));
      
      if (!error && doc.containsKey("relay") && doc.containsKey("state")) {
        uint8_t relayNum = doc["relay"];
        bool state = doc["state"];
        
        if (relayNum < numRelays) {
          setRelayCallback(relayNum, state);
          relayState[relayNum] = state;
          
          DynamicJsonDocument response(256);
          response["success"] = true;
          response["relay"] = relayNum;
          response["state"] = state;
          
          String resp;
          serializeJson(response, resp);
          server.send(200, "application/json", resp);
          return;
        }
      }
    } else if (server.method() == HTTP_GET && server.hasArg("relay")) {
      uint8_t relayNum = server.arg("relay").toInt();
      
      if (relayNum < numRelays) {
        DynamicJsonDocument response(256);
        response["relay"] = relayNum;
        response["state"] = relayState[relayNum];
        
        String resp;
        serializeJson(response, resp);
        server.send(200, "application/json", resp);
        return;
      }
    }
    
    server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  }
};
