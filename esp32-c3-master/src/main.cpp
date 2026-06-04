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
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
      return;
    }
  }
  server.send(200, "text/html", "<h1>BatchFlow Master</h1><p>WiFi AP Running on 192.168.4.1</p>");
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
    Serial.println("      ⚠️  LittleFS mount failed - using default HTML\n");
  } else {
    Serial.println("      ✅ LittleFS initialized");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    Serial.println("      Files available:");
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
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP);
  delay(500);
  
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnet);
  
  // Try AP with WPA2 authentication on channel 1
  bool apOk = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, 1, false, 4);
  Serial.printf("      AP Start: %s\n", apOk ? "✅ SUCCESS" : "❌ FAILED");
  Serial.printf("      SSID: %s\n", WIFI_SSID);
  Serial.printf("      Password: %s\n", WIFI_PASSWORD);
  Serial.printf("      IP Address: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("      Subnet Mask: %s\n", WiFi.softAPSubnetMask().toString().c_str());
  Serial.printf("      MAC Address: %s\n", WiFi.softAPmacAddress().c_str());
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
