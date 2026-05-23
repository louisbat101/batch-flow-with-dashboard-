#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

WebServer server(80);

// Simple in-memory product storage
struct Product {
  String name;
  float ppl;      // Pulses per liter
  float ppg;      // Pulses per gallon
  int closeTime;  // Valve close time in seconds
};

Product products[50];
int productCount = 0;

// ═══════════════════════════════════════════════════════════════════════
// SMART VALVE CLOSING - Calculate flowrate and close early
// ═══════════════════════════════════════════════════════════════════════

struct DispensingStation {
  uint8_t slaveAddr;
  int productIdx;          // Index in products array
  float targetAmount;      // Target liters/gallons
  float targetPulses;      // Target pulses based on PPL/PPG
  uint16_t currentPulses;  // Current pulses received
  float flowRate;          // Pulses per second
  uint32_t startTime;      // Start timestamp
  bool valveOpen;
  bool done;

  // Calculate when to close valve based on flowrate
  void updateFlowRate(uint16_t newPulses) {
    uint32_t elapsed = (millis() - startTime) / 1000;  // seconds
    if (elapsed > 0 && flowRate == 0) {
      flowRate = newPulses / (float)elapsed;  // pulses per second
    }
    currentPulses = newPulses;
    
    if (productIdx < 0 || productIdx >= productCount) return;
    
    // Calculate remaining pulses needed
    float pulsesRemaining = targetPulses - currentPulses;
    
    // Time to close: pulsesRemaining / flowRate (in seconds)
    if (flowRate > 0) {
      float timeToClose = pulsesRemaining / flowRate;
      int closeLeadTime = products[productIdx].closeTime;
      float timeUntilCloseStart = timeToClose - closeLeadTime;
      
      Serial.printf("[Dispense] Addr %u: Flow=%.1f p/s, Remain=%.0f p, Close in %.1f sec\n",
        slaveAddr, flowRate, pulsesRemaining, timeUntilCloseStart);
      
      // If we're within lead time, close valve
      if (timeUntilCloseStart <= 0 && !done) {
        Serial.printf("[Dispense] Addr %u: CLOSING VALVE (%.1f sec early)\n", 
          slaveAddr, -timeUntilCloseStart);
        valveOpen = false;
        done = true;
      }
    }
  }
};

DispensingStation activeStations[10];
int activeCount = 0;

const char HTML[] PROGMEM = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>BatchFlow Master</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { font-family: Arial; background: #1a1a1a; color: #e0e0e0; padding: 20px; }
    .container { max-width: 1200px; margin: 0 auto; }
    h1 { color: #4CAF50; margin: 20px 0; }
    h2 { color: #2196F3; margin: 15px 0; }
    button { padding: 10px 20px; margin: 5px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
    button:hover { background: #45a049; }
    button.danger { background: #f44336; }
    input, select { padding: 8px; margin: 5px; border: 1px solid #444; background: #222; color: #e0e0e0; border-radius: 4px; }
    table { width: 100%; border-collapse: collapse; margin: 20px 0; }
    th, td { padding: 10px; text-align: left; border-bottom: 1px solid #333; }
    th { background: #0d0d0d; color: #4CAF50; }
    .status { padding: 15px; background: #0d0d0d; border-radius: 4px; margin: 15px 0; }
    .page { display: none; }
    .page.active { display: block; }
    .nav { margin: 20px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚙️ BatchFlow Master</h1>
    <div class="nav">
      <button onclick="switchPage('dashboard')">Dashboard</button>
      <button onclick="switchPage('setup')">Setup</button>
      <button onclick="switchPage('load')">Load</button>
      <button onclick="switchPage('run')">Run</button>
    </div>

    <div id="dashboard" class="page active">
      <h2>System Status</h2>
      <div class="status">
        <p><strong>Status:</strong> Ready</p>
        <p><strong>WiFi:</strong> BatchFlow-Master</p>
        <p><strong>Connected Slaves:</strong> <span id="slave-count">0</span></p>
      </div>
    </div>

    <div id="setup" class="page">
      <h2>Product Setup</h2>
      
      <h3>Unit Selection</h3>
      <div class="status">
        <label><input type="radio" name="unit" value="0" checked onchange="changeUnit(0)"> Liters</label>
        <label><input type="radio" name="unit" value="1" onchange="changeUnit(1)"> Gallons</label>
      </div>

      <div class="status">
        <h3>Add Product</h3>
        <input type="text" id="prod-name" placeholder="Product Name" />
        <input type="number" id="prod-pulses" placeholder="Pulses per Liter" value="1000" />
        <input type="number" id="prod-close" placeholder="Valve Close Time (sec)" value="3" min="1" max="30" />
        <button onclick="addProduct()">Add Product</button>
      </div>
      <table>
        <tr><th>#</th><th>Name</th><th id="pulses-header">Pulses / Liter</th><th>Valve Close Time</th><th>Actions</th></tr>
        <tbody id="prod-list"></tbody>
      </table>
    </div>

    <div id="load" class="page">
      <h2>Prepare Load</h2>
      <div id="load-form"></div>
      <button onclick="prepareLoad()">Prepare Load</button>
    </div>

    <div id="run" class="page">
      <h2>Run Dispensing</h2>
      <button onclick="startRun()">Start</button>
      <button onclick="stopRun()" class="danger">Stop</button>
      <div id="run-status" class="status"></div>
    </div>
  </div>

  <script>
    let products = [];
    let currentUnit = 0;

    function switchPage(page) {
      document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
      document.getElementById(page).classList.add('active');
      if (page === 'setup') loadProducts();
      if (page === 'load') setupLoadForm();
    }

    function loadProducts() {
      fetch('/api/products')
        .then(r => r.json())
        .then(data => {
          products = data.products || [];
          let html = '';
          products.forEach((p, i) => {
            const pulsesVal = (currentUnit === 0) ? p.ppl : p.ppg;
            html += '<tr><td>' + (i+1) + '</td><td>' + p.name + '</td><td>' + pulsesVal.toFixed(1) + '</td><td>' + p.closeTime + 's</td><td><button onclick="delProduct(' + i + ')">Delete</button></td></tr>';
          });
          document.getElementById('prod-list').innerHTML = html;
        });
    }

    function addProduct() {
      const name = document.getElementById('prod-name').value;
      const pulses = parseFloat(document.getElementById('prod-pulses').value);
      const closeTime = parseInt(document.getElementById('prod-close').value);
      
      if (!name || !pulses || !closeTime) {
        alert('Fill all fields');
        return;
      }
      
      let ppl = (currentUnit === 0) ? pulses : (pulses * 3.785);
      let ppg = (currentUnit === 1) ? pulses : (pulses / 3.785);
      
      fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name, ppl, ppg, closeTime })
      }).then(r => {
        if (r.ok) {
          document.getElementById('prod-name').value = '';
          document.getElementById('prod-pulses').value = '1000';
          document.getElementById('prod-close').value = '3';
          loadProducts();
        }
      });
    }

    function loadProducts() {
      fetch('/api/products')
        .then(r => r.json())
        .then(data => {
          products = data.products || [];
          let html = '';
          products.forEach((p, i) => {
            html += '<tr><td>' + p.name + '</td><td>' + p.ppl + '</td><td>' + p.ppg + '</td><td>' + p.closeTime + 's</td><td><button class="danger" onclick="delProduct(' + i + ')">Delete</button></td></tr>';
          });
          document.getElementById('prod-list').innerHTML = html;
        });
    }

    function addProduct() {
      const name = document.getElementById('prod-name').value;
      const ppl = parseFloat(document.getElementById('prod-ppl').value);
      const ppg = parseFloat(document.getElementById('prod-ppg').value);
      const closeTime = parseInt(document.getElementById('prod-close').value);
      
      if (!name || !ppl || !ppg || !closeTime) {
        alert('Fill all fields');
        return;
      }
      
      fetch('/api/products', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name, ppl, ppg, closeTime })
      }).then(r => {
        if (r.ok) {
          document.getElementById('prod-name').value = '';
          document.getElementById('prod-ppl').value = '1000';
          document.getElementById('prod-ppg').value = '264';
          document.getElementById('prod-close').value = '3';
          loadProducts();
        }
      });
    }

    function delProduct(i) {
      fetch('/api/products/' + i, { method: 'DELETE' }).then(() => loadProducts());
    }

    function changeUnit(unit) {
      currentUnit = unit;
      const unitLabel = (unit === 0) ? 'Liters' : 'Gallons';
      const pulsesLabel = (unit === 0) ? 'Pulses / Liter' : 'Pulses / Gallon';
      
      document.getElementById('pulses-header').textContent = pulsesLabel;
      document.getElementById('prod-pulses').placeholder = 'Pulses per ' + unitLabel;
      
      fetch('/api/unit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ unit: unit })
      });
    }

    function setupLoadForm() {
      fetch('/api/products')
        .then(r => r.json())
        .then(data => {
          products = data.products || [];
          let html = '';
          for (let i = 1; i <= 10; i++) {
            html += '<div class="status"><h4>Station ' + i + '</h4>';
            html += '<select id="load-prod-' + i + '"><option value="">-- Select Product --</option>';
            products.forEach((p, idx) => {
              html += '<option value="' + idx + '">' + p.name + '</option>';
            });
            html += '</select>';
            html += '<input type="number" id="load-amt-' + i + '" placeholder="Amount" min="0" step="0.1" />';
            html += '</div>';
          }
          document.getElementById('load-form').innerHTML = html;
        });
    }

    function prepareLoad() { alert('Load prepared'); }
    function startRun() { fetch('/api/run/start', {method:'POST'}); }
    function stopRun() { fetch('/api/run/stop', {method:'POST'}); }
  </script>
</body>
</html>
)html";

void handleRoot() {
  server.send(200, "text/html", HTML);
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["state"] = "OK";
  doc["uptime"] = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleUnitSet() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method not allowed");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  deserializeJson(doc, body);
  
  int unit = doc["unit"] | 0;
  Serial.printf("[API] Unit set to: %s\n", (unit == 0) ? "Liters" : "Gallons");
  
  server.send(200, "text/plain", "OK");
}

void handleProductsList() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("products");
  
  for (int i = 0; i < productCount; i++) {
    JsonObject p = arr.createNestedObject();
    p["name"] = products[i].name;
    p["ppl"] = products[i].ppl;
    p["ppg"] = products[i].ppg;
    p["closeTime"] = products[i].closeTime;
  }
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleProductAdd() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method not allowed");
    return;
  }
  
  if (productCount >= 50) {
    server.send(400, "text/plain", "Max 50 products");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }
  
  Product p;
  p.name = doc["name"].as<String>();
  p.ppl = doc["ppl"] | 1000.0;
  p.ppg = doc["ppg"] | 264.0;
  p.closeTime = doc["closeTime"] | 3;
  
  products[productCount] = p;
  productCount++;
  Serial.printf("[API] Product added: %s (total: %d)\n", p.name.c_str(), productCount);
  
  server.send(200, "text/plain", "OK");
}

void handleProductDelete() {
  String uri = server.uri();
  int idx = atoi(uri.c_str() + 13);  // /api/products/X
  
  if (idx >= 0 && idx < productCount) {
    // Shift array
    for (int i = idx; i < productCount - 1; i++) {
      products[i] = products[i + 1];
    }
    productCount--;
    server.send(200, "text/plain", "Deleted");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

void handleRunStart() {
  Serial.println("[API] Run START");
  server.send(200, "text/plain", "Started");
}

void handleRunStop() {
  Serial.println("[API] Run STOP");
  server.send(200, "text/plain", "Stopped");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n[Master] Booting...");

  // Disable Bluetooth
  btStop();
  
  // Disable WiFi sleep
  WiFi.setSleep(false);

  // Start WiFi in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP("BatchFlow-Master", "batchflow123");
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("[WiFi] AP: BatchFlow-Master\n");
  Serial.printf("[WiFi] IP: %s\n", IP.toString().c_str());

  // Initialize RS-485 (Serial1)
  Serial1.begin(9600, SERIAL_8N1, 9, 21);
  pinMode(20, OUTPUT);
  digitalWrite(20, LOW);
  Serial.println("[RS485] Initialized on Serial1");

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/products", HTTP_GET, handleProductsList);
  server.on("/api/products", HTTP_POST, handleProductAdd);
  server.on("/api/products", HTTP_DELETE, handleProductDelete);
  server.on("/api/unit", HTTP_POST, handleUnitSet);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/run/start", HTTP_POST, handleRunStart);
  server.on("/api/run/stop", HTTP_POST, handleRunStop);
  
  server.begin();
  Serial.println("[Server] Started on port 80");
  Serial.println("[Master] ✅ READY!");
}

void loop() {
  server.handleClient();
  
  // Monitor active dispensing stations and update flowrates
  for (int i = 0; i < activeCount; i++) {
    DispensingStation& station = activeStations[i];
    
    // In a real system, you'd query the slave for current pulse count here
    // For now, we monitor and update
    station.updateFlowRate(station.currentPulses);
    
    // If done, remove from active list
    if (station.done) {
      activeStations[i] = activeStations[activeCount - 1];
      activeCount--;
      i--;
    }
  }
  
  delay(100);
}
