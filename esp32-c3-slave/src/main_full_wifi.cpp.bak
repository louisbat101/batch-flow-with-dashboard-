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
  uint8_t currentAddr = modbusSlave.getSlaveAddress();
  uint32_t pulses = flowMeter.getPulseCount();
  
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>FlowNode</title>";
  html += "<style>";
  html += "body{font-family:sans-serif;background:#667eea;min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0;padding:20px}";
  html += ".container{background:white;padding:30px;border-radius:12px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:500px;width:100%}";
  html += "h1{text-align:center;margin-top:0;font-size:28px}";
  html += ".status-card{background:#f5f5f5;padding:15px;border-radius:8px;margin-bottom:20px}";
  html += ".status-row{display:flex;justify-content:space-between;margin:8px 0}";
  html += ".label{font-weight:600;color:#666}";
  html += ".value{color:#333}";
  html += ".badge{display:inline-block;padding:4px 12px;border-radius:12px;font-size:12px;font-weight:600}";
  html += ".badge-success{background:#4CAF50;color:white}";
  html += ".badge-warning{background:#ff9800;color:white}";
  html += ".section-title{font-size:18px;font-weight:600;margin:20px 0 10px;color:#333}";
  html += "button{padding:15px;margin:5px;border:none;border-radius:6px;cursor:pointer;font-weight:600;width:48%;font-size:14px}";
  html += ".on{background:#4CAF50;color:white}.off{background:#f44336;color:white}";
  html += ".full{width:100%}";
  html += "select,input{width:100%;padding:12px;border:2px solid #ddd;border-radius:6px;font-size:16px;margin:10px 0}";
  html += ".btn-primary{background:#667eea;color:white;width:100%;padding:15px;border:none;border-radius:6px;font-size:16px;font-weight:600;cursor:pointer}";
  html += "</style></head><body><div class='container'>";
  html += "<h1>🌊 FlowNode Control</h1>";
  
  // Status Card
  html += "<div class='status-card'>";
  html += "<div class='status-row'><span class='label'>Slave Address:</span><span class='value'><span class='badge badge-success'>" + String(currentAddr) + "</span></span></div>";
  html += "<div class='status-row'><span class='label'>RS-485 Status:</span><span class='value'><span class='badge badge-success'>READY</span></span></div>";
  html += "<div class='status-row'><span class='label'>Pulse Count:</span><span class='value'>" + String(pulses) + "</span></div>";
  html += "<div class='status-row'><span class='label'>IP Address:</span><span class='value'>192.168.5.1</span></div>";
  html += "</div>";
  
  // Address Configuration
  html += "<div class='section-title'>⚙️ Configuration</div>";
  html += "<form action='/config' method='POST'>";
  html += "<select name='address'>";
  for(int i=1; i<=10; i++) {
    html += "<option value='" + String(i) + "'";
    if(i == currentAddr) html += " selected";
    html += ">Slave Address " + String(i) + "</option>";
  }
  html += "</select>";
  html += "<button type='submit' class='btn-primary'>💾 Save Address & Restart</button>";
  html += "</form>";
  
  // Relay Control
  html += "<div class='section-title'>🔌 Relay Control</div>";
  html += "<div>";
  for(int i=1; i<=4; i++) {
    html += "<button class='on' onclick=\"fetch('/relay/" + String(i) + "/on').then(()=>setTimeout(()=>location.reload(),100))\">Relay " + String(i) + " ON</button>";
    html += "<button class='off' onclick=\"fetch('/relay/" + String(i) + "/off').then(()=>setTimeout(()=>location.reload(),100))\">Relay " + String(i) + " OFF</button>";
  }
  html += "<br><button class='full on' onclick=\"fetch('/all/on').then(()=>setTimeout(()=>location.reload(),100))\">ALL ON</button>";
  html += "<button class='full off' onclick=\"fetch('/all/off').then(()=>setTimeout(()=>location.reload(),100))\">ALL OFF</button>";
  html += "</div></div></body></html>";
  server.send(200, "text/html", html);
}

void handleConfig() {
  if (server.method() == HTTP_POST) {
    String newAddrStr = server.arg("address");
    uint8_t newAddr = newAddrStr.toInt();
    
    if (newAddr >= MIN_SLAVE_ADDR && newAddr <= MAX_SLAVE_ADDR) {
      // Save to NVS (persistent storage)
      modbusSlave.setSlaveAddress(newAddr);
      
      String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
      html += "<title>Config Saved</title>";
      html += "<style>body{font-family:sans-serif;background:#667eea;min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0;padding:20px}";
      html += ".container{background:white;padding:40px;border-radius:12px;box-shadow:0 20px 60px rgba(0,0,0,.3);max-width:400px;text-align:center}";
      html += "h1{color:#4CAF50}p{font-size:18px;color:#666}</style></head><body>";
      html += "<div class='container'><h1>✓ Configuration Saved!</h1>";
      html += "<p>Slave address changed to <strong>" + String(newAddr) + "</strong></p>";
      html += "<p>Device will restart in 3 seconds...</p>";
      html += "<script>setTimeout(()=>{window.location='/'},3000)</script>";
      html += "</div></body></html>";
      server.send(200, "text/html", html);
      
      delay(3000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Invalid address (must be 1-10)");
    }
  } else {
    server.send(405, "text/plain", "Method not allowed");
  }
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
  flowMeter.begin();
  
  // Initialize Modbus slave on RS-485
  modbusSlave.begin();

  Serial.println("\n\n╔════════════════════════════════════════════╗");
  Serial.println("║   ESP32-C3 Dispensing Slave Node v3.0      ║");
  Serial.println("║   WiFi + RS-485 + LED + Flowmeter          ║");
  Serial.println("╚════════════════════════════════════════════╝");

  // Full WiFi reset sequence for ESP32-C3
  Serial.println("[WiFi] Resetting WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);

  // Set AP mode FIRST, then disable sleep
  Serial.println("[WiFi] Setting AP mode...");
  WiFi.mode(WIFI_AP);
  delay(500);

  // Disable power saving AFTER mode is set (C3 resets this on mode change)
  WiFi.setSleep(false);
  delay(200);

  // Configure IP BEFORE starting SoftAP - Use different IP from master
  Serial.println("[WiFi] Configuring IP...");
  IPAddress local_ip(192, 168, 5, 1);      // Different from master (192.168.4.1)
  IPAddress gateway(192, 168, 5, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  delay(300);

  // Start SoftAP - channel 11 avoids master on channel 6
  Serial.println("[WiFi] Starting SoftAP...");
  bool ok = WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASS, 11, false, 4);
  Serial.printf("[WiFi] softAP result: %s\n", ok ? "SUCCESS" : "FAILED");

  // Give it time to stabilize and start beaconing
  delay(3000);
  Serial.printf("[WiFi] Channel: %d\n", WiFi.channel());
  
  int stations = WiFi.softAPgetStationNum();
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] IP: %s | Stations: %d\n", ip.toString().c_str(), stations);

  Serial.println("[Server] Registering handlers...");
  server.on("/", handleRoot);
  server.on("/config", HTTP_POST, handleConfig);
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
  
  // Sync flowmeter pulse count to Modbus
  modbusSlave.setPulseCount(flowMeter.getPulseCount());
  
  // LED status updates
  statusLED.update();
  
  delay(5);
}
