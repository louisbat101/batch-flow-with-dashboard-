#include <Arduino.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "FlowMeter.h"
#include "RS485Valve.h"

// Pins (adjust per your wiring)
constexpr int FLOW1_PIN = 34; // pulse input (must be an input-only pin)
constexpr int FLOW2_PIN = 35;

// RS485 Serial
HardwareSerial RS485Serial(2); // UART2
constexpr int RS485_RX = 16;
constexpr int RS485_TX = 17;
constexpr int RS485_DE_RE = 4; // DE+RE control

AsyncWebServer server(80);
Preferences prefs;

FlowMeter flow1(FLOW1_PIN);
FlowMeter flow2(FLOW2_PIN);
RS485Valve valve1(RS485Serial, RS485_DE_RE, 1); // Modbus ID 1
RS485Valve valve2(RS485Serial, RS485_DE_RE, 2); // Modbus ID 2

// Runtime state
volatile bool batching = false;
float targetLiters = 0.0;
int activeValve = 0; // 1 or 2

void IRAM_ATTR onFlow1() { flow1.pulseISR(); }
void IRAM_ATTR onFlow2() { flow2.pulseISR(); }

String readFileAsString(const char* path) {
  File f = SPIFFS.open(path, "r");
  if(!f) return String();
  String s;
  while(f.available()) s += (char)f.read();
  f.close();
  return s;
}

void setupRoutes() {
  // Serve web UI
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  server.serveStatic("/app.js", SPIFFS, "/app.js");
  server.serveStatic("/styles.css", SPIFFS, "/styles.css");

  // API: get status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(512);
    doc["flow1_litres"] = flow1.litres();
    doc["flow2_litres"] = flow2.litres();
    doc["batching"] = batching;
    doc["target_litres"] = targetLiters;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // API: start batch { "valve":1, "litres": 2.5 }
  server.on("/api/start", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!request->hasParam("body", true)) { request->send(400); return; }
    String body = request->getParam("body", true)->value();
    DynamicJsonDocument doc(256);
    deserializeJson(doc, body);
    int valve = doc["valve"] | 0;
    float litres = doc["litres"] | 0.0;
    if(valve < 1 || valve > 2 || litres <= 0.0) { request->send(400); return; }
    // reset counters
    flow1.resetLitres();
    flow2.resetLitres();
    batching = true;
    targetLiters = litres;
    activeValve = valve;
    // open the selected valve
    if(valve == 1) valve1.open(); else valve2.open();
    request->send(200, "application/json", "{\"started\":true}");
  });

  // API: stop
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request){
    batching = false;
    valve1.close();
    valve2.close();
    request->send(200, "application/json", "{\"stopped\":true}");
  });

  // API: products list
  server.on("/api/products", HTTP_GET, [](AsyncWebServerRequest *request){
    String s = readFileAsString("/products.json");
    if(s.length()==0) request->send(500, "application/json", "{}");
    else request->send(200, "application/json", s);
  });

  // API: calibration (set pulses-per-litre for a flowmeter)
  server.on("/api/calibrate", HTTP_POST, [](AsyncWebServerRequest *request){
    if(!request->hasParam("body", true)) { request->send(400); return; }
    String body = request->getParam("body", true)->value();
    DynamicJsonDocument doc(256);
    deserializeJson(doc, body);
    int meter = doc["meter"] | 0;
    float ppl = doc["pulses_per_litre"] | 0.0;
    if(meter==1) flow1.setPulsesPerLitre(ppl);
    else if(meter==2) flow2.setPulsesPerLitre(ppl);
    else { request->send(400); return; }
    // persist
    prefs.begin("cal", false);
    prefs.putFloat(meter==1?"f1_ppl":"f2_ppl", ppl);
    prefs.end();
    request->send(200, "application/json", "{\"ok\":true}");
  });

}

void setup() {
  Serial.begin(115200);
  delay(1000);
  // SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
  }

  // Load preferences for calibration
  prefs.begin("cal", false);
  float f1p = prefs.getFloat("f1_ppl", 450.0); // default pulses per litre
  float f2p = prefs.getFloat("f2_ppl", 450.0);
  prefs.end();
  flow1.setPulsesPerLitre(f1p);
  flow2.setPulsesPerLitre(f2p);

  // Setup flowmeter interrupts
  pinMode(FLOW1_PIN, INPUT_PULLUP);
  pinMode(FLOW2_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW1_PIN), onFlow1, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW2_PIN), onFlow2, FALLING);

  // RS485 UART init
  RS485Serial.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  valve1.begin();
  valve2.begin();

  // Start WiFi in AP mode for first-run or serve from existing
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("BatchFlow-ESP32");

  setupRoutes();
  server.begin();

  Serial.println("Setup complete");
}

void loop() {
  if(batching) {
    // check flow for active valve
    float measured = (activeValve==1) ? flow1.litres() : flow2.litres();
    if(measured >= targetLiters) {
      // reached target
      if(activeValve==1) valve1.close(); else valve2.close();
      batching = false;
      Serial.println("Batch complete");
    }
  }
  delay(50);
}
/*
 *  Batching System – ESP32-WROOM Master Firmware
 *  RS-485 master → communicates with C3 slave nodes
 *  WiFi AP · Web server · Batching logic
 */

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "config.h"
#include "database.h"
#include "settings.h"
#include "rs485_master.h"
#include "batching.h"
#include "webserver.h"

Database  db;
Settings  settings;
Batching  batch;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n============================");
    Serial.println("   Batch Loader  v1.0");
    Serial.println("============================\n");

    // ── Filesystem ───────────────────────────────────
    if (!LittleFS.begin(true)) {
        Serial.println("[ERR] LittleFS failed");
        return;
    }
    Serial.println("[OK] LittleFS");

    // ── Product database ─────────────────────────────
    db.begin();
    Serial.printf("[OK] Database – %d products\n", db.count);

    // ── Settings ─────────────────────────────────────
    settings.begin();
    Serial.printf("[OK] Settings – unit: %s\n", settings.unit);

    // ── Hardware ─────────────────────────────────────
    RS485Master::begin();
    Serial.println("[OK] RS-485 Master");

    // Ping slave nodes to check connectivity
    if (RS485Master::ping(SLAVE1_ADDR))
        Serial.println("[OK] Slave 1 (Line 1) responded");
    else
        Serial.println("[WARN] Slave 1 (Line 1) not responding");

    if (RS485Master::ping(SLAVE2_ADDR))
        Serial.println("[OK] Slave 2 (Line 2) responded");
    else
        Serial.println("[WARN] Slave 2 (Line 2) not responding");

    // ── Batching engine ──────────────────────────────
    batch.begin(&db, &settings);
    Serial.println("[OK] Batching engine");

    // ── WiFi AP ──────────────────────────────────────
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    delay(300);
    Serial.printf("[OK] WiFi AP  SSID: %s  IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

    // ── Web server ───────────────────────────────────
    WebServer::init();
    Serial.println("[OK] Web server on port 80");
    Serial.println("\n--- READY ---\n");
}

void loop() {
    batch.update();
    delay(10);
}
