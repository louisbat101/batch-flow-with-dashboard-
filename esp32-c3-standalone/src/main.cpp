#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "BatchController.h"
#include "FlowMeter.h"
#include "RS485ValveController.h"
#include "Database.h"
#include "embedded_web.h"

// Global objects
WebServer server(80);
DualHallFlowMeter flowMeter(FLOW_HALL_A_PIN, FLOW_HALL_B_PIN);
RS485ValveController valves(Serial1, RS485_DE_RE_PIN);  // Use Serial1 for ESP32-C3
Database database;
BatchController batchController(flowMeter, valves, database);

// State variables
unsigned long lastUpdate = 0;
bool ledState = false;

// Forward declarations
void setupWebServer();
void handleStatus();
void handleGetProducts();
void handleAddProduct();
void handleStartBatch();
void handleStopBatch();
void handleValveControl();
void handleGetSettings();
void handleSaveSettings();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("BatchFlow ESP32-C3 Master/Slave Controller Starting...");
    
    // Initialize LEDs
    pinMode(LED_POWER_PIN, OUTPUT);
    pinMode(LED_STATUS_PIN, OUTPUT);
    digitalWrite(LED_POWER_PIN, HIGH);  // Power LED always on
    
    // Initialize Serial1 for RS485
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    
    // Initialize filesystem
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    Serial.println("LittleFS mounted successfully");
    
    // Initialize hardware
    flowMeter.begin();
    valves.begin();
    database.begin();
    
    // Setup WiFi Access Point
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.println("WiFi Access Point started: " + String(AP_SSID));
    Serial.println("IP address: " + WiFi.softAPIP().toString());
    
    // Setup web server routes
    setupWebServer();
    
    server.begin();
    Serial.println("Web server started - BatchFlow Controller Ready!");
}

void setupWebServer() {
    // Main dashboard
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", index_html);
    });
    
    // API endpoints
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/products", HTTP_GET, handleGetProducts);
    server.on("/api/products", HTTP_POST, handleAddProduct);
    server.on("/api/batch/start", HTTP_POST, handleStartBatch);
    server.on("/api/batch/stop", HTTP_POST, handleStopBatch);
    server.on("/api/valves", HTTP_POST, handleValveControl);
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings", HTTP_POST, handleSaveSettings);
}

void handleStatus() {
    // Use the existing getStatusJson method
    String statusJson = batchController.getStatusJson();
    server.send(200, "application/json", statusJson);
}

void handleGetProducts() {
    JsonDocument doc;
    JsonArray products = doc["products"].to<JsonArray>();
    
    for (int i = 0; i < database.count; i++) {
        JsonObject prod = products.add<JsonObject>();
        prod["id"] = i;
        prod["name"] = database.products[i].name;
        prod["targetLitres"] = database.products[i].targetLitres;
        prod["calibration"] = database.products[i].calibration;
        prod["closeTime"] = database.products[i].closeTime;
        prod["valve"] = database.products[i].valve;
    }
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleAddProduct() {
    if (database.count >= MAX_PRODUCTS) {
        server.send(400, "text/plain", "Maximum products reached");
        return;
    }
    
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    Product* prod = &database.products[database.count];
    strlcpy(prod->name, doc["name"] | "New Product", sizeof(prod->name));
    prod->targetLitres = doc["targetLitres"] | 1.0f;
    prod->calibration = doc["calibration"] | DEFAULT_PULSES_PER_LITRE;
    prod->closeTime = doc["closeTime"] | DEFAULT_CLOSE_TIME_MS;
    prod->valve = doc["valve"] | 1;
    
    database.count++;
    database.save();
    
    server.send(200, "text/plain", "Product added");
}

void handleStartBatch() {
    int productId = server.arg("productId").toInt();
    
    if (batchController.startBatch(productId)) {
        server.send(200, "text/plain", "Batch started");
    } else {
        server.send(400, "text/plain", "Failed to start batch");
    }
}

void handleStopBatch() {
    batchController.stopBatch();
    server.send(200, "text/plain", "Batch stopped");
}

void handleValveControl() {
    int valve = server.arg("valve").toInt();
    String action = server.arg("action");
    
    if (valve < 1 || valve > 2) {
        server.send(400, "text/plain", "Invalid valve number");
        return;
    }
    
    if (action == "open") {
        valves.openValve(valve);
        server.send(200, "text/plain", "Valve " + String(valve) + " opened");
    } else if (action == "close") {
        valves.closeValve(valve);
        server.send(200, "text/plain", "Valve " + String(valve) + " closed");
    } else {
        server.send(400, "text/plain", "Invalid action");
    }
}

void handleGetSettings() {
    JsonDocument doc;
    doc["pulsesPerLitre"] = flowMeter.getPulsesPerLitre();
    doc["closeTime"] = DEFAULT_CLOSE_TIME_MS;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleSaveSettings() {
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    if (doc["pulsesPerLitre"].is<float>()) {
        flowMeter.setPulsesPerLitre(doc["pulsesPerLitre"]);
    }
    
    server.send(200, "text/plain", "Settings saved");
}

void loop() {
    server.handleClient();
    
    // Update batch controller
    batchController.update();
    
    // Check valve safety
    valves.checkSafety();
    
    // Handle status LED
    if (millis() - lastUpdate > 500) {
        lastUpdate = millis();
        
        if (batchController.getState() == BATCHING) {
            ledState = !ledState;
            digitalWrite(LED_STATUS_PIN, ledState);
        } else if (batchController.getState() == ERROR) {
            // Fast blink for error
            ledState = !ledState;
            digitalWrite(LED_STATUS_PIN, ledState);
            delay(100);
        } else {
            digitalWrite(LED_STATUS_PIN, LOW);
        }
    }
}