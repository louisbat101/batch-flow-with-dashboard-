#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "esp_task_wdt.h"

// WiFi AP Configuration
#define WIFI_SSID         "BatchFlow-Master"
#define WIFI_PASSWORD     "batchflow123"

// UART Configuration for Teensy Link
// Teensy Pin 1 (RX1) ← C3 GPIO 21 (TX)
// Teensy Pin 2 (TX1) ← C3 GPIO 20 (RX)
#define UART_RXD_PIN      20        // GPIO 20 - receives from Teensy
#define UART_TXD_PIN      21        // GPIO 21 - sends to Teensy
#define UART_BAUD         115200    // UART baud rate
#define UART_BUFFER_SIZE  256

HardwareSerial uartTeensy(1);  // UART1 on ESP32-C3
uint8_t uartRxBuffer[UART_BUFFER_SIZE];
int uartRxIndex = 0;

String teensyVersion = "unknown";  // FW version from Teensy

WebServer server(80);

// Debug UART messages (set to 0 to quiet logging)
#define DEBUG_UART  0

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
  // Try to load products from LittleFS
  productCount = 0;
  if (LittleFS.exists("/products.json")) {
    File f = LittleFS.open("/products.json", "r");
    if (f) {
      DynamicJsonDocument doc(2048);
      DeserializationError err = deserializeJson(doc, f);
      if (!err) {
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
          if (productCount >= 10) break;
          products[productCount].id = obj["id"] | (productCount + 1);
          products[productCount].name = obj["name"] | "Product";
          products[productCount].pulsesPerLiter = obj["ppl"] | 450.0f;
          products[productCount].valveTime = obj["valveTime"] | 0.25f;
          productCount++;
        }
        Serial.printf("✅ Products loaded: %d\n", productCount);
      }
      f.close();
    }
  }
  if (productCount == 0) {
    // Default products
    products[0] = {1, "Acid", 450.0, 0.25};
    products[1] = {2, "Caustic", 375.0, 0.30};
    products[2] = {3, "Rinse Water", 500.0, 0.20};
    products[3] = {4, "Additive", 1000.0, 0.15};
    productCount = 4;
    Serial.printf("✅ Default products loaded: %d\n", productCount);
  }
}

void saveProducts() {
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < productCount; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["id"] = products[i].id;
    obj["name"] = products[i].name;
    obj["ppl"] = products[i].pulsesPerLiter;
    obj["valveTime"] = products[i].valveTime;
  }
  File f = LittleFS.open("/products.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
    Serial.println("✅ Products saved");
  }
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

// ── Send UART frame to Teensy ──────────────────────────
void sendUARTCommand(uint8_t cmd, const uint8_t* data, uint8_t len) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < len; i++) checksum ^= data[i];
  
  uartTeensy.write(0xFF);       // MSG_START
  uartTeensy.write(cmd);        // command
  uartTeensy.write(len);        // data length
  uartTeensy.write(data, len);  // payload
  uartTeensy.write(checksum);   // XOR checksum
  uartTeensy.write(0xFE);       // MSG_END
  uartTeensy.flush();
}

void handleControl() {
  if (!server.hasArg("board") || !server.hasArg("action")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  int boardAddr = server.arg("board").toInt();
  String action = server.arg("action");
  float litres = server.arg("litres").toFloat();
  
  // Build control frame matching Teensy's expected format:
  // [boardAddr][action][productId=0][targetLiters(4B float)]
  uint8_t data[7];
  data[0] = boardAddr;
  data[1] = (action == "start") ? 1 : 0;
  data[2] = 0;  // productId = 0 for now
  
  // Pack target litres as 4-byte float (big-endian IEEE 754)
  uint32_t targetBits;
  memcpy(&targetBits, &litres, 4);
  data[3] = (targetBits >> 24) & 0xFF;
  data[4] = (targetBits >> 16) & 0xFF;
  data[5] = (targetBits >> 8) & 0xFF;
  data[6] = targetBits & 0xFF;
  
  sendUARTCommand(0x02, data, 7);  // CMD_CONTROL = 0x02
  
  Serial.printf("[Control] Forwarded to Teensy: board=%d action=%s litres=%.2f\n",
                boardAddr, action.c_str(), litres);
  
  DynamicJsonDocument doc(256);
  doc["status"] = "ok";
  doc["board"] = boardAddr;
  doc["action"] = action;
  doc["forwarded"] = true;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ═══════════════════════════════════════════════════════
// UART MESSAGE PARSING (from Teensy)
// ═══════════════════════════════════════════════════════

void parseUARTMessage(uint8_t *msg, int len) {
  if (len < 3) return;  // Too short
  
  uint8_t cmd = msg[0];
  uint8_t dataLen = msg[1];
  
  if (cmd == 0x01) {  // Status Report from Teensy
    if (dataLen >= 15) {
      uint8_t boardAddr = msg[2];
      if (boardAddr >= 1 && boardAddr <= 4) {
        Board &board = boards[boardAddr - 1];
        
        board.address = boardAddr;
        board.online = (msg[3] != 0);  // status byte
        board.dispensing = (msg[3] == 2);  // 2 = dispensing
        
        // Read target liters (4-byte float)
        memcpy(&board.targetAmount, &msg[4], 4);
        
        // Read dispensed liters (4-byte float)
        memcpy(&board.dispensedAmount, &msg[8], 4);
        
        // Valve state
        bool valveOpen = msg[12] != 0;
        
        // Pulse count (4-byte uint32)
        memcpy(&board.pulseCount, &msg[13], 4);
        
        board.lastPollTime = millis();
        
        if (DEBUG_UART)
          Serial.printf("[UART] Board %d: online=%d, target=%.1f L, dispensed=%.1f L, pulses=%d\n",
            boardAddr, board.online, board.targetAmount, board.dispensedAmount, board.pulseCount);
      }
    }
  } else if (cmd == 0x81 && dataLen >= 1 && msg[2] == 0) {
    // CMD_ACK with type=0: version string
    char ver[16] = {0};
    int vlen = dataLen - 1;
    if (vlen > 15) vlen = 15;
    memcpy(ver, &msg[3], vlen);
    teensyVersion = String(ver);
    Serial.printf("[UART] Teensy firmware version: %s\n", ver);
  }
}

void readUARTMessages() {
  while (uartTeensy.available()) {
    uint8_t byte = uartTeensy.read();
    
    if (byte == 0xFF && uartRxIndex == 0) {
      // Start of message
      uartRxBuffer[uartRxIndex++] = byte;
    } else if (uartRxIndex > 0) {
      uartRxBuffer[uartRxIndex++] = byte;
      
      // Check for end marker
      if (byte == 0xFE && uartRxIndex >= 4) {
        // Parse complete message (skip start 0xFF and end 0xFE)
        uint8_t cmd = uartRxBuffer[1];
        uint8_t dataLen = uartRxBuffer[2];
        
        // Full frame: [0xFF][CMD][LEN][DATA...][CHECKSUM][0xFE]
        // Total = 1 + 1 + 1 + dataLen + 1 + 1 = dataLen + 5 bytes
        // uartRxIndex is the count of bytes read (index after last byte)
        if (uartRxIndex == (dataLen + 5)) {
          parseUARTMessage(&uartRxBuffer[1], dataLen + 1);
        }
        
        uartRxIndex = 0;  // Reset for next message
      }
      
      // Prevent buffer overflow
      if (uartRxIndex >= UART_BUFFER_SIZE) {
        uartRxIndex = 0;
      }
    }
  }
}

// ── API: List Products ─────────────────────────────────
void handleListProducts() {
  DynamicJsonDocument doc(2048);
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

// ── API: Save Products ─────────────────────────────────
void handleSaveProducts() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  productCount = 0;
  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    if (productCount >= 10) break;
    products[productCount].id = obj["id"] | (productCount + 1);
    products[productCount].name = obj["name"] | "Product";
    products[productCount].pulsesPerLiter = obj["pulsesPerLiter"] | 450.0f;
    products[productCount].valveTime = obj["valveTime"] | 0.25f;
    productCount++;
  }
  saveProducts();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// ── API: Start Run ─────────────────────────────────────
void handleRunStart() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  DynamicJsonDocument doc(256);
  deserializeJson(doc, server.arg("plain"));
  
  int boardAddr = doc["address"] | 0;
  float litres = doc["litres"] | 0.0f;
  
  if (boardAddr < 1 || boardAddr > 4 || litres <= 0) {
    server.send(400, "text/plain", "Invalid params");
    return;
  }
  
  // Forward as start command to Teensy
  uint8_t data[7];
  data[0] = boardAddr;
  data[1] = 1;  // action = start
  data[2] = 0;  // productId
  uint32_t targetBits;
  memcpy(&targetBits, &litres, 4);
  data[3] = (targetBits >> 24) & 0xFF;
  data[4] = (targetBits >> 16) & 0xFF;
  data[5] = (targetBits >> 8) & 0xFF;
  data[6] = targetBits & 0xFF;
  sendUARTCommand(0x02, data, 7);
  
  Serial.printf("[Run] Start board %d: %.2f L\n", boardAddr, litres);
  server.send(200, "application/json", "{\"status\":\"started\"}");
}

// ── API: Stop Run ──────────────────────────────────────
void handleRunStop() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  DynamicJsonDocument doc(256);
  deserializeJson(doc, server.arg("plain"));
  
  int boardAddr = doc["address"] | 0;
  if (boardAddr < 1 || boardAddr > 4) {
    server.send(400, "text/plain", "Invalid board");
    return;
  }
  
  // Forward as stop command to Teensy
  uint8_t data[7];
  data[0] = boardAddr;
  data[1] = 0;  // action = stop
  data[2] = 0;
  data[3] = 0; data[4] = 0; data[5] = 0; data[6] = 0;  // target = 0
  sendUARTCommand(0x02, data, 7);
  
  Serial.printf("[Run] Stop board %d\n", boardAddr);
  server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

// ── API: Firmware Version ──────────────────────────────
void handleVersion() {
  // Version is stored when received from Teensy via UART
  DynamicJsonDocument doc(128);
  doc["version"] = teensyVersion;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Disable USB JTAG to free up GPIO 20/21
  #if CONFIG_IDF_TARGET_ESP32C3
  ESP_LOGI("init", "Disabling USB JTAG interface to use GPIO 20/21");
  #endif
  
  Serial.println("\n\n════════════════════════════════════════════════════");
  Serial.println("        BATCH FLOW MASTER - WiFi Only");
  Serial.println("════════════════════════════════════════════════════\n");
  
  // ── Watchdog: 10 second timeout ───────────────────────
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);
  Serial.println("      ✅ Watchdog enabled (10s)\n");
  
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
  
  // Initialize UART link to Teensy
  Serial.println("[2b/4] Initializing UART link to Teensy...");
  uartTeensy.begin(UART_BAUD, SERIAL_8N1, UART_RXD_PIN, UART_TXD_PIN);
  uartTeensy.setRxBufferSize(UART_BUFFER_SIZE);
  Serial.printf("      ✅ UART2 running at %d baud (RX=GPIO%d, TX=GPIO%d)\n", 
    UART_BAUD, UART_RXD_PIN, UART_TXD_PIN);
  Serial.println();
  Serial.println("[3/4] Starting WiFi AP...");
  
  // Disable power saving features
  WiFi.setSleep(false);
  WiFi.persistent(false);
  
  // Reset WiFi completely
  WiFi.disconnect(true);
  delay(1000);
  
  // Set AP mode FIRST, then set TX power (must have AP/STA started first)
  WiFi.mode(WIFI_AP);
  delay(500);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Max power (after mode is set)
  
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
  Serial.printf("      SSID: %s\n", WIFI_SSID);
  Serial.printf("      Password: %s\n", WIFI_PASSWORD);
  Serial.printf("      IP Address: 192.168.4.1\n");
  Serial.println();
  delay(1000);
  
  // Setup web server routes
  Serial.println("[4/4] Setting up web server...");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/boards/status", HTTP_GET, handleBoardsStatus);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/run/start", HTTP_POST, handleRunStart);
  server.on("/api/run/stop", HTTP_POST, handleRunStop);
  server.on("/api/products", HTTP_GET, handleListProducts);
  server.on("/api/products", HTTP_POST, handleSaveProducts);
  server.on("/api/version", HTTP_GET, handleVersion);
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
  
  // Read incoming UART messages from Teensy
  readUARTMessages();
  
  // Print connection status periodically
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {  // Print every 10 seconds
    lastPrint = millis();
    int connectedClients = WiFi.softAPgetStationNum();
    Serial.printf("[%lu] Connected clients: %d\n", millis()/1000, connectedClients);
  }
  
  esp_task_wdt_reset();  // Feed watchdog
  
  yield();  // Let other tasks run
}
