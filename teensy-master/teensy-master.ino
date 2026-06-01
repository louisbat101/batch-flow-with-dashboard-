/* ══════════════════════════════════════════════════════/* ══════════════════════════════════════════════════════

   BATCH FLOW MASTER – Teensy 4.1 with RS-485   BATCH FLOW MASTER – Teensy 4.1 with RS-485

   ══════════════════════════════════════════════════════ */   ══════════════════════════════════════════════════════ */



#include <Arduino.h>#include <Arduino.h>

#include <ArduinoJson.h>

// ═══════════════════════════════════════════════════════

// RS-485 CONFIGURATION – TEENSY 4.1// ═══════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════// RS-485 CONFIGURATION – TEENSY 4.1

#define RS485_RXD_PIN     9       // RX2 on Teensy 4.1// ═══════════════════════════════════════════════════════

#define RS485_TXD_PIN     10      // TX2 on Teensy 4.1#define RS485_RXD_PIN     9       // RX2 on Teensy 4.1

#define RS485_DE_PIN      11      // DE/RE (Direction Control)#define RS485_TXD_PIN     10      // TX2 on Teensy 4.1

#define RS485_BAUD        9600    // Modbus RTU baud rate#define RS485_DE_PIN      11      // DE/RE (Direction Control)

#define RS485_BAUD        9600    // Modbus RTU baud rate

// ═══════════════════════════════════════════════════════

// BOARD & PRODUCT DATA STRUCTURESWebServer server(80);

// ═══════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════

struct Product {// BOARD & PRODUCT DATA STRUCTURES

  int id;// ═══════════════════════════════════════════════════════

  String name;

  float pulsesPerLiter;struct Product {

  float valveTime;  int id;

};  String name;

  float pulsesPerLiter;

struct Board {  float valveTime;

  int address;};

  String name;

  String product;struct Board {

  bool online;  int address;

  bool dispensing;  String name;

  float dispensedAmount;  String product;

  float targetAmount;  bool online;

  int pulseCount;  bool dispensing;

  unsigned long lastPollTime;  float dispensedAmount;

};  float targetAmount;

  int pulseCount;

Product products[10];  unsigned long lastPollTime;

int productCount = 0;};



Board boards[4];Product products[10];

int boardCount = 4;int productCount = 0;



// ═══════════════════════════════════════════════════════Board boards[4];

// RS-485 MODBUS FUNCTIONSint boardCount = 4;

// ═══════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════

uint16_t crc16(uint8_t* data, int len) {// RS-485 MODBUS FUNCTIONS

  uint16_t crc = 0xFFFF;// ═══════════════════════════════════════════════════════

  for (int i = 0; i < len; i++) {

    crc ^= data[i];uint16_t crc16(uint8_t* data, int len) {

    for (int j = 0; j < 8; j++) {  uint16_t crc = 0xFFFF;

      if (crc & 1) crc = (crc >> 1) ^ 0xA001;  for (int i = 0; i < len; i++) {

      else crc = crc >> 1;    crc ^= data[i];

    }    for (int j = 0; j < 8; j++) {

  }      if (crc & 1) crc = (crc >> 1) ^ 0xA001;

  return crc;      else crc = crc >> 1;

}    }

  }

bool queryBoardStatus(int slaveAddr) {  return crc;

  // Modbus RTU Read Input Registers (FC 04)}

  uint8_t request[8];

  request[0] = slaveAddr;bool queryBoardStatus(int slaveAddr) {

  request[1] = 0x04;        // Function Code 04  // Modbus RTU Read Input Registers (FC 04)

  request[2] = 0x00;        // Register address high  uint8_t request[8];

  request[3] = 0x00;        // Register address low  request[0] = slaveAddr;

  request[4] = 0x00;        // Count high  request[1] = 0x04;        // Function Code 04

  request[5] = 0x04;        // Count low (read 4 registers)  request[2] = 0x00;        // Register address high

    request[3] = 0x00;        // Register address low

  uint16_t crc = crc16(request, 6);  request[4] = 0x00;        // Count high

  request[6] = crc & 0xFF;  request[5] = 0x04;        // Count low (read 4 registers)

  request[7] = (crc >> 8) & 0xFF;  

    uint16_t crc = crc16(request, 6);

  // Send request - NO BLOCKING DELAYS  request[6] = crc & 0xFF;

  digitalWrite(RS485_DE_PIN, HIGH);  // TX mode  request[7] = (crc >> 8) & 0xFF;

  delayMicroseconds(500);  

    // Send request - NO BLOCKING DELAYS

  Serial2.write(request, 8);  digitalWrite(RS485_DE_PIN, HIGH);  // TX mode

  Serial2.flush();  delayMicroseconds(500);

    

  delayMicroseconds(500);  Serial2.write(request, 8);

  digitalWrite(RS485_DE_PIN, LOW);   // RX mode  Serial2.flush();

    

  // Read response with non-blocking pattern  delayMicroseconds(500);

  uint8_t response[32];  digitalWrite(RS485_DE_PIN, LOW);   // RX mode

  int bytesRead = 0;  

  unsigned long startTime = millis();  // Read response with non-blocking pattern

    uint8_t response[32];

  while (millis() - startTime < 300) {  int bytesRead = 0;

    if (Serial2.available()) {  unsigned long startTime = millis();

      response[bytesRead] = Serial2.read();  

      bytesRead++;  while (millis() - startTime < 300) {

      if (bytesRead >= 32) break;    yield();  // CRITICAL: Let WiFi run

    }    

  }    if (Serial2.available()) {

        response[bytesRead] = Serial2.read();

  // Validate response      bytesRead++;

  if (bytesRead < 9) {      if (bytesRead >= 32) break;

    Serial.printf("[RS485] No response from board %d\n", slaveAddr);    }

    return false;  }

  }  

    // Validate response

  if (response[0] != slaveAddr || response[1] != 0x04) {  if (bytesRead < 9) {

    Serial.printf("[RS485] Invalid response from board %d\n", slaveAddr);    Serial.printf("[RS485] No response from board %d\n", slaveAddr);

    return false;    return false;

  }  }

    

  uint16_t respCrc = crc16(response, bytesRead - 2);  if (response[0] != slaveAddr || response[1] != 0x04) {

  uint16_t recvCrc = (response[bytesRead-1] << 8) | response[bytesRead-2];    Serial.printf("[RS485] Invalid response from board %d\n", slaveAddr);

      return false;

  if (respCrc != recvCrc) {  }

    Serial.printf("[RS485] CRC error from board %d\n", slaveAddr);  

    return false;  uint16_t respCrc = crc16(response, bytesRead - 2);

  }  uint16_t recvCrc = (response[bytesRead-1] << 8) | response[bytesRead-2];

    

  Serial.printf("[RS485] ✅ Board %d responded\n", slaveAddr);  if (respCrc != recvCrc) {

  return true;    Serial.printf("[RS485] CRC error from board %d\n", slaveAddr);

}    return false;

  }

void pollAllBoards() {  

  Serial.println("[RS485] ═══ Starting board poll cycle ═══");  Serial.printf("[RS485] ✅ Board %d responded\n", slaveAddr);

  for (int i = 0; i < boardCount; i++) {  return true;

    int slaveAddr = boards[i].address;}

    bool wasOnline = boards[i].online;

    void pollAllBoards() {

    Serial.printf("[RS485] Querying board %d (address %d)...\n", i+1, slaveAddr);  Serial.println("[RS485] ═══ Starting board poll cycle ═══");

    boards[i].online = queryBoardStatus(slaveAddr);  for (int i = 0; i < boardCount; i++) {

        int slaveAddr = boards[i].address;

    if (boards[i].online != wasOnline) {    bool wasOnline = boards[i].online;

      Serial.printf("[STATUS] Board %d changed to %s\n", slaveAddr,     

                    boards[i].online ? "ONLINE" : "OFFLINE");    Serial.printf("[RS485] Querying board %d (address %d)...\n", i+1, slaveAddr);

    }    boards[i].online = queryBoardStatus(slaveAddr);

        

    if (boards[i].online) {    if (boards[i].online != wasOnline) {

      boards[i].lastPollTime = millis();      Serial.printf("[STATUS] Board %d changed to %s\n", slaveAddr, 

    }                    boards[i].online ? "ONLINE" : "OFFLINE");

        }

    delayMicroseconds(100);    

  }    if (boards[i].online) {

  Serial.println("[RS485] ═══ Poll cycle complete ═══");      boards[i].lastPollTime = millis();

}    }

    

// ═══════════════════════════════════════════════════════    yield();  // CRITICAL: Let WiFi run between queries

// INITIALIZATION    delayMicroseconds(100);

// ═══════════════════════════════════════════════════════  }

  Serial.println("[RS485] ═══ Poll cycle complete ═══");

void initializeBoards() {}

  boards[0] = {1, "Station 1 - Acid", "Acid", false, false, 0, 10.0, 0, 0};

  boards[1] = {2, "Station 2 - Caustic", "Caustic", false, false, 0, 15.0, 0, 0};// ═══════════════════════════════════════════════════════

  boards[2] = {3, "Station 3 - Rinse Water", "Rinse Water", false, false, 0, 20.0, 0, 0};// WEB SERVER HANDLERS

  boards[3] = {4, "Station 4 - Additive", "Additive", false, false, 0, 5.0, 0, 0};// ═══════════════════════════════════════════════════════

}

void handleRoot() {

void initializeRS485() {  server.send(200, "text/html", "<h1>Batch Flow Master (Teensy 4.1)</h1><p>RS-485 Master Board</p>");

  Serial.println("[*] Initializing RS-485 communication...");}

  

  pinMode(RS485_DE_PIN, OUTPUT);void handleHealth() {

  digitalWrite(RS485_DE_PIN, LOW);  // Start in RX mode  StaticJsonDocument<128> doc;

    doc["status"] = "ok";

  // Configure Serial2 for RS-485 (RX2=pin9, TX2=pin10)  doc["board"] = "Teensy 4.1";

  Serial2.begin(RS485_BAUD);  doc["board2_online"] = boards[1].online;

  delay(100);  

    String json;

  Serial.printf("      RXD Pin: %d\n", RS485_RXD_PIN);  serializeJson(doc, json);

  Serial.printf("      TXD Pin: %d\n", RS485_TXD_PIN);  server.send(200, "application/json", json);

  Serial.printf("      DE Pin:  %d\n", RS485_DE_PIN);}

  Serial.printf("      Baud:    %d\n", RS485_BAUD);

  Serial.println("      ✅ RS-485 ready\n");void handleBoardsStatus() {

}  StaticJsonDocument<2048> doc;

  JsonArray boardsArray = doc.createNestedArray();

void setup() {  

  Serial.begin(115200);  for (int i = 0; i < boardCount; i++) {

  delay(2000);    JsonObject b = boardsArray.createNestedObject();

      b["address"] = boards[i].address;

  Serial.println("\n\n════════════════════════════════════════════════════");    b["name"] = boards[i].name;

  Serial.println("        BATCH FLOW MASTER – TEENSY 4.1");    b["product"] = boards[i].product;

  Serial.println("════════════════════════════════════════════════════\n");    b["online"] = boards[i].online;  // Current status from last poll

      b["dispensing"] = boards[i].dispensing;

  // Initialize data    b["dispensedAmount"] = boards[i].dispensedAmount;

  initializeBoards();    b["targetAmount"] = boards[i].targetAmount;

      b["pulseCount"] = boards[i].pulseCount;

  // Initialize RS-485  }

  initializeRS485();  

    String json;

  Serial.println("[Status] Ready to poll boards via RS-485\n");  serializeJson(doc, json);

  Serial.println("════════════════════════════════════════════════════");  server.send(200, "application/json", json);

  Serial.println("           ✅ BATCH FLOW MASTER READY");}

  Serial.println("════════════════════════════════════════════════════\n");

}// ═══════════════════════════════════════════════════════

// INITIALIZATION

void loop() {// ═══════════════════════════════════════════════════════

  // Background RS-485 polling

  static unsigned long lastPoll = 0;void initializeBoards() {

  if (millis() - lastPoll > 1000) {  boards[0] = {1, "Station 1 - Acid", "Acid", false, false, 0, 10.0, 0, 0};

    lastPoll = millis();  boards[1] = {2, "Station 2 - Caustic", "Caustic", false, false, 0, 15.0, 0, 0};

    pollAllBoards();  boards[2] = {3, "Station 3 - Rinse Water", "Rinse Water", false, false, 0, 20.0, 0, 0};

  }  boards[3] = {4, "Station 4 - Additive", "Additive", false, false, 0, 5.0, 0, 0};

  }

  // Print status periodically

  static unsigned long lastPrint = 0;void initializeRS485() {

  if (millis() - lastPrint > 15000) {  Serial.println("[*] Initializing RS-485 communication...");

    lastPrint = millis();  

    Serial.printf("\n[%lu] Board Status Summary\n", millis()/1000);  pinMode(RS485_DE_PIN, OUTPUT);

    for (int i = 0; i < boardCount; i++) {  digitalWrite(RS485_DE_PIN, LOW);  // Start in RX mode

      Serial.printf("  Board %d (%s): %s - %s\n", boards[i].address, boards[i].name.c_str(),  

                    boards[i].online ? "🟢 ONLINE" : "🔴 OFFLINE",  // Configure Serial2 for RS-485

                    boards[i].dispensing ? "DISPENSING" : "IDLE");  Serial2.begin(RS485_BAUD);

    }  delay(100);

    Serial.println();  

  }  Serial.printf("      RXD Pin: %d\n", RS485_RXD_PIN);

    Serial.printf("      TXD Pin: %d\n", RS485_TXD_PIN);

  delay(10);  Serial.printf("      DE Pin:  %d\n", RS485_DE_PIN);

}  Serial.printf("      Baud:    %d\n", RS485_BAUD);

  Serial.println("      ✅ RS-485 ready\n");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n════════════════════════════════════════════════════");
  Serial.println("        BATCH FLOW MASTER – TEENSY 4.1");
  Serial.println("════════════════════════════════════════════════════\n");
  
  // Initialize data
  initializeBoards();
  
  // Initialize RS-485
  initializeRS485();
  
  // Configure WiFi AP
  Serial.println("[1/3] Configuring WiFi AP...");
  WiFi.mode(WIFI_AP);
  delay(100);
  
  IPAddress apIP(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, subnet);
  
  bool apOk = WiFi.softAP("BatchFlow-Master", "batchflow123", 6, false, 4);
  Serial.printf("      AP Start: %s\n", apOk ? "✅ SUCCESS" : "❌ FAILED");
  Serial.printf("      IP Address: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.println();
  
  // Setup web server routes
  Serial.println("[2/3] Setting up web server routes...");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/health", HTTP_GET, handleHealth);
  server.on("/api/boards/status", HTTP_GET, handleBoardsStatus);
  server.begin();
  Serial.println("      ✅ API routes registered\n");
  
  Serial.println("[3/3] Starting main loop...\n");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("           ✅ BATCH FLOW MASTER READY");
  Serial.println("════════════════════════════════════════════════════");
  Serial.println("\n📡 WiFi AP:  BatchFlow-Master");
  Serial.println("🔐 Password: batchflow123");
  Serial.println("🌐 Access:   http://192.168.4.1/");
  Serial.println("📊 API:      http://192.168.4.1/api/boards/status");
  Serial.println("════════════════════════════════════════════════════\n");
  
  delay(1000);
}

void loop() {
  server.handleClient();
  yield();  // CRITICAL: Let WiFi run
  
  // Background RS-485 polling
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll > 1000) {
    lastPoll = millis();
    pollAllBoards();
  }
  
  yield();  // CRITICAL: Let WiFi run again
  
  // Print status periodically
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 15000) {
    lastPrint = millis();
    int connectedClients = WiFi.softAPgetStationNum();
    Serial.printf("\n[%lu] Connected: %d client(s)\n", millis()/1000, connectedClients);
    for (int i = 0; i < boardCount; i++) {
      Serial.printf("  Board %d (%s): %s - %s\n", boards[i].address, boards[i].name.c_str(),
                    boards[i].online ? "ONLINE" : "OFFLINE",
                    boards[i].dispensing ? "DISPENSING" : "IDLE");
    }
    Serial.println();
  }
  
  delayMicroseconds(100);
}
