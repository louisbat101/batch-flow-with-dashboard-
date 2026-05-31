#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <FS.h>

WebServer server(80);

// ═══════════════════════════════════════════════════════
// BOARD & PRODUCT DATA STRUCTURES
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
};

Product products[10];
int productCount = 0;

Board boards[4];
int boardCount = 4;

// ═══════════════════════════════════════════════════════
// API HANDLERS
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
  server.send(200, "text/html", "<h1>BatchFlow Master</h1><p>LittleFS files not found. Uploading...</p>");
}

void handleStaticFile() {
  String path = server.uri();
  String contentType = "application/octet-stream";
  
  if (path.endsWith(".js")) {
    contentType = "application/javascript";
  } else if (path.endsWith(".css")) {
    contentType = "text/css";
  } else if (path.endsWith(".html")) {
    contentType = "text/html";
  }
  
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    if (file) {
      server.streamFile(file, contentType);
      file.close();
      return;
    }
  }
  
  // Fallback to index.html for SPA routing
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
      return;
    }
  }
  
  server.send(404, "text/plain", "Not Found");
}

// GET /api/boards/status - Get all boards and their status
void handleBoardsStatus() {
  StaticJsonDocument<2048> doc;
  JsonArray boardsArray = doc.createNestedArray();
  
  for (int i = 0; i < boardCount; i++) {
    JsonObject b = boardsArray.createNestedObject();
    b["address"] = boards[i].address;
    b["name"] = boards[i].name;
    b["product"] = boards[i].product;
    b["online"] = boards[i].online;
    b["dispensing"] = boards[i].dispensing;
    b["dispensedAmount"] = boards[i].dispensedAmount;
    b["targetAmount"] = boards[i].targetAmount;
    b["pulseCount"] = boards[i].pulseCount;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// POST /api/boards/rename - Rename a board
void handleBoardsRename() {
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int address = doc["address"] | -1;
  String newName = doc["name"] | "";
  
  if (address < 0 || address >= boardCount || newName.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Invalid parameters\"}");
    return;
  }
  
  boards[address - 1].name = newName;
  server.send(200, "application/json", "{\"status\":\"OK\"}");
}

// GET /api/products - Get all products
void handleProductsList() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("products");
  
  for (int i = 0; i < productCount; i++) {
    JsonObject p = arr.createNestedObject();
    p["id"] = products[i].id;
    p["name"] = products[i].name;
    p["pulsesPerLiter"] = products[i].pulsesPerLiter;
    p["valveTime"] = products[i].valveTime;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// POST /api/products - Add a new product
void handleProductAdd() {
  if (productCount >= 10) {
    server.send(400, "application/json", "{\"error\":\"Max 10 products\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  products[productCount].id = productCount + 1;
  products[productCount].name = doc["name"] | "Unknown";
  products[productCount].pulsesPerLiter = doc["pulsesPerLiter"] | 450.0;
  products[productCount].valveTime = doc["valveTime"] | 0.25;
  
  productCount++;
  server.send(200, "application/json", "{\"status\":\"OK\"}");
}

// PUT /api/products/{id} - Update a product
void handleProductUpdate() {
  String path = server.uri();
  int id = path.substring(path.lastIndexOf('/') + 1).toInt();
  
  if (id < 1 || id > productCount) {
    server.send(404, "application/json", "{\"error\":\"Product not found\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  Product& p = products[id - 1];
  if (doc.containsKey("name")) {
    p.name = String(doc["name"].as<const char*>());
  }
  if (doc.containsKey("pulsesPerLiter")) {
    p.pulsesPerLiter = doc["pulsesPerLiter"];
  }
  if (doc.containsKey("valveTime")) {
    p.valveTime = doc["valveTime"];
  }
  
  server.send(200, "application/json", "{\"status\":\"OK\"}");
}

// DELETE /api/products/{id} - Delete a product
void handleProductDelete() {
  String path = server.uri();
  int id = path.substring(path.lastIndexOf('/') + 1).toInt();
  
  if (id < 1 || id > productCount) {
    server.send(404, "application/json", "{\"error\":\"Product not found\"}");
    return;
  }
  
  for (int i = id - 1; i < productCount - 1; i++) {
    products[i] = products[i + 1];
    products[i].id--;
  }
  productCount--;
  
  server.send(200, "application/json", "{\"status\":\"OK\"}");
}

// POST /api/run/start - Start dispensing on a station
void handleRunStart() {
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int address = doc["address"] | -1;
  
  if (address < 1 || address > boardCount) {
    server.send(400, "application/json", "{\"error\":\"Invalid address\"}");
    return;
  }
  
  boards[address - 1].dispensing = true;
  boards[address - 1].dispensedAmount = 0;
  boards[address - 1].pulseCount = 0;
  
  Serial.printf("[RUN] Starting board %d\n", address);
  server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Dispensing started\"}");
}

// POST /api/run/stop - Stop dispensing on a station
void handleRunStop() {
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int address = doc["address"] | -1;
  
  if (address < 1 || address > boardCount) {
    server.send(400, "application/json", "{\"error\":\"Invalid address\"}");
    return;
  }
  
  boards[address - 1].dispensing = false;
  
  Serial.printf("[RUN] Stopped board %d\n", address);
  server.send(200, "application/json", "{\"status\":\"OK\",\"message\":\"Dispensing stopped\"}");
}

// GET /api/status - General system status
void handleStatus() {
  StaticJsonDocument<512> doc;
  doc["state"] = "OK";
  doc["uptime"] = millis() / 1000;
  doc["connectedStations"] = WiFi.softAPgetStationNum();
  
  int onlineBoards = 0;
  for (int i = 0; i < boardCount; i++) {
    if (boards[i].online) onlineBoards++;
  }
  doc["onlineBoards"] = onlineBoards;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════
// INITIALIZATION
// ═══════════════════════════════════════════════════════

void initializeBoards() {
  // Initialize 4 stations with simulated online/offline status
  boards[0] = {1, "Station 1 - Acid", "Acid", true, false, 0, 10.0, 0};
  boards[1] = {2, "Station 2 - Caustic", "Caustic", true, false, 0, 15.0, 0};
  boards[2] = {3, "Station 3 - Rinse Water", "Rinse Water", false, false, 0, 20.0, 0};
  boards[3] = {4, "Station 4 - Additive", "Additive", true, false, 0, 5.0, 0};
}

void initializeProducts() {
  products[0] = {1, "Acid", 450.0, 0.250};
  products[1] = {2, "Caustic", 375.0, 0.300};
  products[2] = {3, "Rinse Water", 500.0, 0.200};
  products[3] = {4, "Additive", 1000.0, 0.150};
  productCount = 4;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n════════════════════════════════════════════════════");
  Serial.println("        BATCH FLOW MASTER - STARTUP");
  Serial.println("════════════════════════════════════════════════════\n");
  
  // Initialize data
  initializeBoards();
  initializeProducts();
  
  // Initialize LittleFS
  Serial.println("[1/6] Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("      ❌ LittleFS FAILED - will use default HTML\n");
  } else {
    Serial.println("      ✅ LittleFS initialized");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    Serial.println("      Files available:");
    int fileCount = 0;
    while (file && fileCount < 20) {
      Serial.printf("        ✓ %s (%d bytes)\n", file.name(), file.size());
      fileCount++;
      file = root.openNextFile();
    }
    Serial.println();
  }
  delay(500);
  
  // Disable Bluetooth
  Serial.println("[2/6] Disabling Bluetooth...");
  btStop();
  Serial.println("      ✅ Done\n");
  
  // Disable WiFi sleep
  Serial.println("[3/6] Configuring WiFi settings...");
  WiFi.setSleep(false);
  delay(500);
  Serial.println("      ✅ WiFi sleep disabled\n");

  // Start WiFi in AP mode
  Serial.println("[4/6] Starting WiFi AP...");
  WiFi.mode(WIFI_AP);
  delay(500);
  
  bool apOk = WiFi.softAP("BatchFlow-Master", "batchflow123", 6, false, 4);
  Serial.printf("      AP Start: %s\n", apOk ? "✅ SUCCESS" : "❌ FAILED");
  Serial.printf("      IP Address: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("      Subnet Mask: %s\n", WiFi.softAPSubnetMask().toString().c_str());
  Serial.printf("      Max Clients: 4\n\n");
  delay(1000);
  
  // Setup web server routes
  Serial.println("[5/6] Setting up web server routes...");
  
  // Static files
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/app.js", HTTP_GET, handleStaticFile);
  server.on("/dashboard.js", HTTP_GET, handleStaticFile);
  server.on("/batching.js", HTTP_GET, handleStaticFile);
  server.on("/style.css", HTTP_GET, handleStaticFile);
  server.on("/batching.html", HTTP_GET, handleStaticFile);
  
  // API Routes - Boards
  server.on("/api/boards/status", HTTP_GET, handleBoardsStatus);
  server.on("/api/boards/rename", HTTP_POST, handleBoardsRename);
  
  // API Routes - Products
  server.on("/api/products", HTTP_GET, handleProductsList);
  server.on("/api/products", HTTP_POST, handleProductAdd);
  server.on("/api/products/1", HTTP_PUT, handleProductUpdate);
  server.on("/api/products/2", HTTP_PUT, handleProductUpdate);
  server.on("/api/products/3", HTTP_PUT, handleProductUpdate);
  server.on("/api/products/4", HTTP_PUT, handleProductUpdate);
  server.on("/api/products/1", HTTP_DELETE, handleProductDelete);
  server.on("/api/products/2", HTTP_DELETE, handleProductDelete);
  server.on("/api/products/3", HTTP_DELETE, handleProductDelete);
  server.on("/api/products/4", HTTP_DELETE, handleProductDelete);
  
  // API Routes - Run/Dispense
  server.on("/api/run/start", HTTP_POST, handleRunStart);
  server.on("/api/run/stop", HTTP_POST, handleRunStop);
  
  // API Routes - Status
  server.on("/api/status", HTTP_GET, handleStatus);
  
  // Catch-all for SPA
  server.onNotFound(handleStaticFile);
  
  server.begin();
  Serial.println("      ✅ 12 API routes registered\n");
  
  // Start monitoring
  Serial.println("[6/6] Starting main loop...");
  Serial.println("      ✅ Ready for connections\n");
  
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("           ✅ BATCH FLOW MASTER READY");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("\n📡 WiFi AP:  BatchFlow-Master");
  Serial.println("🔐 Password: batchflow123");
  Serial.println("🌐 Access:   http://192.168.4.1/");
  Serial.println("📊 API:      http://192.168.4.1/api/boards/status");
  Serial.println("════════════════════════════════════════════════════\n");
}

void loop() {
  server.handleClient();
  
  // Simulate random online/offline changes occasionally
  static unsigned long lastToggle = 0;
  if (millis() - lastToggle > 30000) {  // Every 30 seconds
    lastToggle = millis();
    // Randomly update board 3 status
    if (random(2) == 0) {
      boards[2].online = !boards[2].online;
      Serial.printf("[STATUS] Board 3 is now %s\n", boards[2].online ? "ONLINE" : "OFFLINE");
    }
  }
  
  // Simulate dispensing progress for running boards
  static unsigned long lastDispense = 0;
  if (millis() - lastDispense > 1000) {
    lastDispense = millis();
    
    for (int i = 0; i < boardCount; i++) {
      if (boards[i].dispensing && boards[i].online) {
        // Simulate pulse counting (e.g., 450 pulses per liter)
        boards[i].pulseCount += random(10, 50);  // Simulate flowmeter pulses
        boards[i].dispensedAmount = boards[i].pulseCount / 450.0;  // Convert to liters
        
        // Auto-stop when target reached
        if (boards[i].dispensedAmount >= boards[i].targetAmount) {
          boards[i].dispensing = false;
          boards[i].dispensedAmount = boards[i].targetAmount;
          Serial.printf("[COMPLETE] Board %d finished: %.2f L\n", boards[i].address, boards[i].targetAmount);
        }
      }
    }
  }
  
  // Print status periodically
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    lastPrint = millis();
    int connectedClients = WiFi.softAPgetStationNum();
    Serial.printf("\n[%lu] Connected: %d client(s)\n", millis()/1000, connectedClients);
    for (int i = 0; i < boardCount; i++) {
      if (boards[i].online) {
        Serial.printf("  Board %d: %s - %s\n", boards[i].address, boards[i].name.c_str(), 
                      boards[i].dispensing ? "DISPENSING" : "IDLE");
      }
    }
    Serial.println();
  }
  
  delay(1);
}
