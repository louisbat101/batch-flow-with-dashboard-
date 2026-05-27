#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* MASTER_SSID = "BatchFlow-Master";
const char* MASTER_PASS = "batchflow123";
const char* AP_SSID = "FlowNode-Setup";
const char* AP_PASS = "flownode123";
WebServer server(80);
bool relayState = false;

void handleStatus() {
  DynamicJsonDocument doc(512);
  doc["online"] = true;
  JsonArray relays = doc.createNestedArray("relays");
  relays.add(relayState);
  relays.add(false);
  relays.add(false);
  relays.add(false);
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetRelay() {
  if (server.hasArg("relay") && server.hasArg("state")) {
    int relay = server.arg("relay").toInt();
    bool state = (server.arg("state") == "1");
    if (relay == 0) relayState = state;
    Serial.printf("[Relay] Set relay %d to %s\n", relay, state ? "ON" : "OFF");
  }
  handleStatus();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[SLAVE] Boot starting...");
  
  Serial.println("[WiFi] Setting mode STA...");
  WiFi.mode(WIFI_STA);
  delay(500);
  
  Serial.println("[WiFi] Disabling sleep...");
  WiFi.setSleep(false);
  delay(200);
  
  // Connect to master's WiFi with static IP
  Serial.printf("[WiFi] Connecting to master: %s (pass: %s)\n", MASTER_SSID, MASTER_PASS);
  
  // Set static IP: 192.168.4.2
  IPAddress staticIP(192, 168, 4, 2);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(staticIP, gateway, subnet);
  
  WiFi.begin(MASTER_SSID, MASTER_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.printf(".");
    attempts++;
    if (attempts % 10 == 0) Serial.printf(" [%d/40]\n", attempts);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] ✓ Connected to master network\n");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.printf("\n[WiFi] ✗ Connection failed! Status: %d\n", WiFi.status());
  }
  
  Serial.println("[Server] Registering handlers...");
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/relay", HTTP_GET, handleSetRelay);
  
  Serial.println("[Server] Starting...");
  server.begin();
  
  Serial.println("[SLAVE] ✓ READY");
}

void loop() {
  server.handleClient();
  delay(10);
}
