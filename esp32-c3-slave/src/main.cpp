#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "slave_config.h"
#include "neopixel_led.h"
#include "flowmeter.h"
#include "modbus_slave.h"

static const uint8_t RELAY_PINS[] = {RELAY_CH1_PIN, RELAY_CH2_PIN, RELAY_CH3_PIN, RELAY_CH4_PIN};
static const size_t NUM_RELAYS = sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]);

WebServer server(80);
static bool relayState[NUM_RELAYS] = {false, false, false, false};

// Global instances
NeoPixelLED statusLED;
FlowMeter flowMeter;
ModbusSlave modbusSlave(DEFAULT_SLAVE_ADDR);

// Flowmeter ISR singleton - define it here
FlowMeter* flowmeterInstance = &flowMeter;

static void setRelay(uint8_t ch, bool on) {
  if (ch < NUM_RELAYS) {
    relayState[ch] = on;
    // ACTIVE-HIGH: relay ON when pin is HIGH, OFF when LOW
    digitalWrite(RELAY_PINS[ch], on ? HIGH : LOW);
    Serial.printf("[Relay %u] %s (GPIO%u=%d)\n", ch + 1, on ? "ON" : "OFF", RELAY_PINS[ch], on ? HIGH : LOW);
    
    // Sync relay state to Modbus slave
    modbusSlave.setRelayState(ch, on);
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>FlowNode</title><style>body{font-family:sans-serif;background:#667eea;min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0}.container{background:white;padding:40px;border-radius:12px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:500px}h1{text-align:center;margin-top:0}button{padding:15px;margin:5px;border:none;border-radius:6px;cursor:pointer;font-weight:600;width:48%;font-size:14px}.on{background:#4CAF50;color:white}.off{background:#f44336;color:white}.full{width:100%}</style></head><body><div class='container'><h1>🌊 FlowNode Relay Control</h1>";
  html += "<div>";
  for(int i=1; i<=4; i++) {
    html += "<button class='on' onclick=\"fetch('/relay/" + String(i) + "/on')\">Relay " + String(i) + " ON</button>";
    html += "<button class='off' onclick=\"fetch('/relay/" + String(i) + "/off')\">Relay " + String(i) + " OFF</button>";
  }
  html += "<br><button class='full on' onclick=\"fetch('/all/on')\">ALL ON</button>";
  html += "<button class='full off' onclick=\"fetch('/all/off')\">ALL OFF</button>";
  html += "</div></div></body></html>";
  server.send(200, "text/html", html);
}

void handleRelay() {
  String uri = server.uri();
  Serial.printf("[RELAY] URI='%s'\n", uri.c_str());

  // Parse /relay/<num>/<cmd> manually (sscanf can be unreliable)
  if (!uri.startsWith("/relay/")) {
    Serial.println("[RELAY] ERROR: Bad prefix");
    server.send(400, "text/plain", "Bad");
    return;
  }

  String rest = uri.substring(7); // Skip "/relay/"
  int slash = rest.indexOf('/');
  if (slash < 0) {
    Serial.println("[RELAY] ERROR: No command");
    server.send(400, "text/plain", "Bad");
    return;
  }

  String chStr = rest.substring(0, slash);
  String cmd = rest.substring(slash + 1);
  
  int ch = chStr.toInt();
  cmd.toLowerCase();
  bool on = (cmd == "on");

  Serial.printf("[RELAY] ch=%d cmd='%s' on=%d\n", ch, cmd.c_str(), on);

  if (ch < 1 || ch > (int)NUM_RELAYS) {
    Serial.printf("[RELAY] ERROR: Invalid channel %d\n", ch);
    server.send(400, "text/plain", "Bad");
    return;
  }

  setRelay(ch - 1, on);
  server.send(200, "text/plain", "OK");
}

void handleAll() {
  String uri = server.uri();
  Serial.printf("[HTTP] handleAll called with URI='%s'\n", uri.c_str());
  
  bool on = (uri == "/all/on");
  Serial.printf("[HTTP] parsed on=%d\n", on ? 1 : 0);
  
  for (size_t i = 0; i < NUM_RELAYS; i++) setRelay(i, on);
  server.send(200, "text/plain", "OK");
}

void setup() {
  delay(500);
  Serial.begin(115200);
  delay(500);
  Serial.setDebugOutput(true);
  
  // Initialize LED first (booting state)
  statusLED.begin();
  statusLED.setColor(NeoPixelLED::COLOR_BOOT);
  statusLED.setBlinking(NeoPixelLED::COLOR_BOOT, 500);
  
  // Initialize relay pins
  for (size_t i = 0; i < NUM_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  // Initialize flowmeter
  FlowMeter::flowmeterInstance = &flowMeter;
  flowMeter.begin();
  
  // Initialize Modbus slave on RS-485
  modbusSlave.begin();

  Serial.println("\n\n╔════════════════════════════════════════════╗");
  Serial.println("║   ESP32-C3 Dispensing Slave Node v3.0      ║");
  Serial.println("║   WiFi + RS-485 + LED + Flowmeter          ║");
  Serial.println("╚════════════════════════════════════════════╝");

  // Disable Bluetooth FIRST
  Serial.println("[BT] Stopping Bluetooth...");
  btStop();
  delay(500);
  
  // Disable power saving
  Serial.println("[WiFi] Disabling sleep mode...");
  WiFi.setSleep(false);
  delay(100);
  
  // Turn off any existing WiFi
  WiFi.mode(WIFI_OFF);
  delay(500);
  
  // Now set AP mode
  Serial.println("[WiFi] Setting AP mode...");
  WiFi.mode(WIFI_AP);
  delay(500);
  
  // Configure IP BEFORE starting SoftAP
  Serial.println("[WiFi] Configuring IP...");
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(200);
  
  // Start SoftAP with 8+ char password, channel 1, no hidden, 4 max clients
  Serial.println("[WiFi] Starting SoftAP...");
  bool ok = WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASS, 1, false, 4);
  Serial.printf("[WiFi] softAP result: %s\n", ok ? "SUCCESS" : "FAILED");
  
  // Give it time to stabilize
  delay(2000);
  
  int stations = WiFi.softAPgetStationNum();
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] IP: %s | Stations: %d\n", ip.toString().c_str(), stations);

  Serial.println("[Server] Registering handlers...");
  server.on("/", handleRoot);
  server.onNotFound([]() {
    if (server.uri().startsWith("/relay/")) handleRelay();
    else if (server.uri() == "/all/on" || server.uri() == "/all/off") handleAll();
    else server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("[Server] ✓ READY");
  Serial.printf("[✓ Connect] SSID: '%s' | Pass: '%s'\n", CONFIG_AP_SSID, CONFIG_AP_PASS);
  Serial.printf("[✓ URL] http://%s\n", ip.toString().c_str());
  
  // LED: Switch to idle state
  statusLED.setColor(NeoPixelLED::COLOR_IDLE);
  Serial.println("[LED] Status: IDLE (Cyan)");
}

void loop() {
  // WiFi web server
  server.handleClient();
  
  // Modbus RTU slave (RS-485)
  modbusSlave.update();
  
  // Flowmeter updates
  flowMeter.update();
  
  // LED status updates
  statusLED.update();
  
  delay(5);
}
