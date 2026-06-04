#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// WiFi AP Configuration
#define WIFI_SSID         "BatchFlow-Master"
#define WIFI_PASSWORD     "batchflow123"

WebServer server(80);

// ═══════════════════════════════════════════════════════
// DATA STRUCTURES
// ═══════════════════════════════════════════════════════

struct Product {
  int id;
  String name;
  float pulsesPerLiter;
  float valveTime;  // seconds
};

struct Board {
  int address;
  String name;
  String product;
  bool online;
  bool dispensing;
  float dispensedAmount;
  float targetAmount;
  int pulseCount;
  unsigned long lastPollTime;
};

Product products[10];
int productCount = 0;

Board boards[4];
int boardCount = 4;

// ═══════════════════════════════════════════════════════
// INITIALIZATION FUNCTIONS
// ═══════════════════════════════════════════════════════

void initializeBoards() {
  for (int i = 0; i < boardCount; i++) {
    boards[i].address = i + 1;
    boards[i].name = "Board " + String(i + 1);
    boards[i].product = "None";
    boards[i].online = false;
    boards[i].dispensing = false;
    boards[i].dispensedAmount = 0;
    boards[i].targetAmount = 0;
    boards[i].pulseCount = 0;
    boards[i].lastPollTime = 0;
  }
  Serial.println("✅ Boards initialized");
}

void initializeProducts() {
  productCount = 0;
  Serial.println("✅ Products initialized");
}

// ═══════════════════════════════════════════════════════
// WEB SERVER HANDLERS
// ═══════════════════════════════════════════════════════

void handleRoot() {
  Serial.println("[Web] GET / requested");
  
  if (LittleFS.exists("/index.html")) {
    Serial.println("[Web] ✅ Found /index.html - serving");
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
      return;
    } else {
      Serial.println("[Web] ❌ Could not open /index.html");
    }
  } else {
    Serial.println("[Web] ❌ /index.html does not exist!");
  }
  
  // Fallback if files not found
  server.send(200, "text/html", "<h1>BatchFlow Master</h1><p>✅ Web Server WORKS!</p><p>But /index.html not found in LittleFS</p>");
}

void handleBoardsStatus() {
  DynamicJsonDocument doc(1024);
  JsonArray boardsArray = doc.createNestedArray("boards");
  
  for (int i = 0; i < boardCount; i++) {
    JsonObject boardObj = boardsArray.createNestedObject();
    boardObj["address"] = boards[i].address;
    boardObj["name"] = boards[i].name;
    boardObj["online"] = boards[i].online;
    boardObj["dispensing"] = boards[i].dispensing;
    boardObj["dispensedAmount"] = boards[i].dispensedAmount;
    boardObj["targetAmount"] = boards[i].targetAmount;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleControl() {
  if (!server.hasArg("board") || !server.hasArg("action")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  int boardAddr = server.arg("board").toInt();
  String action = server.arg("action");
  
  DynamicJsonDocument doc(256);
  doc["status"] = "ok";
  doc["board"] = boardAddr;
  doc["action"] = action;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ═══════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n════════════════════════════════════════════════════");
  Serial.println("        BATCH FLOW MASTER - WiFi Only");
  Serial.println("════════════════════════════════════════════════════\n");
  
  // Initialize data
  initializeBoards();
  initializeProducts();
  
  // Initialize LittleFS
  Serial.println("[1/4] Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("      ❌ LittleFS FAILED - mount error\n");
  } else {
    Serial.println("      ✅ LittleFS initialized");
    
    // Check for index.html
    if (LittleFS.exists("/index.html")) {
      Serial.println("      ✅ Found /index.html");
    } else {
      Serial.println("      ❌ /index.html NOT FOUND - need to upload filesystem!");
    }
    
    // List files
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    Serial.println("      Files in LittleFS:");
    int fileCount = 0;
    while (file && fileCount < 20) {
      Serial.printf("        • %s (%d bytes)\n", file.name(), file.size());
      fileCount++;
      file = root.openNextFile();
    }
    Serial.println();
  }
  delay(500);
  
  // Disable Bluetooth (free up resources)
  Serial.println("[2/4] Disabling Bluetooth...");
  btStop();
  Serial.println("      ✅ Done\n");
  
  // Configure WiFi
  Serial.println("[3/4] Starting WiFi AP...");
  
  // Disable power saving features
  WiFi.setSleep(false);
  WiFi.persistent(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Max power
  
  // Reset WiFi completely
  WiFi.disconnect(true);
  delay(1000);
  
  // Set AP mode
  WiFi.mode(WIFI_AP);
  delay(500);
  
  // Configure IP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, gateway, subnet);
  delay(500);
  
  // Start AP - try multiple times if needed
  Serial.println("      Attempting to start AP...");
  bool apOk = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    apOk = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 1, false);
    Serial.printf("      Attempt %d: %s\n", attempt + 1, apOk ? "✅" : "❌");
    if (apOk) break;
    delay(500);
  }
  
  delay(3000);  // Wait for AP to fully start
  
  Serial.println();
  Serial.printf("      AP Final Status: %s\n", apOk ? "✅ BROADCASTING" : "❌ FAILED");
  Serial.printf("      SSID: %s (should appear on tablet WiFi list)\n", WiFi.softAPSSID().c_str());
  Serial.printf("      Password: %s\n", WIFI_PASSWORD);
  Serial.printf("      IP Address: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("      Clients: %d\n", WiFi.softAPgetStationNum());
  Serial.println();
  delay(1000);
  
  // Setup web server routes
  Serial.println("[4/4] Setting up web server...");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/boards/status", HTTP_GET, handleBoardsStatus);
  server.on("/api/control", HTTP_POST, handleControl);
  server.serveStatic("/", LittleFS, "/");
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("      ✅ Web server started on port 80\n");
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("              🚀 READY FOR CONNECTIONS");
  Serial.println("════════════════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════════════════
// MAIN LOOP
// ═══════════════════════════════════════════════════════

void loop() {
  // Handle web server client requests
  server.handleClient();
  
  // Print connection status periodically
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {  // Print every 10 seconds
    lastPrint = millis();
    int connectedClients = WiFi.softAPgetStationNum();
    Serial.printf("[%lu] Connected clients: %d\n", millis()/1000, connectedClients);
  }
  
  yield();  // Let other tasks run
}
