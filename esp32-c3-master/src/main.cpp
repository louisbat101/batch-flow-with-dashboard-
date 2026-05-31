#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <FS.h>

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
    input, select { padding: 8px; margin: 5px; border: 1px solid #444; background: #222; color: #e0e0e0; border-radius: 4px; }
    table { width: 100%; border-collapse: collapse; margin: 20px 0; }
    th, td { padding: 10px; text-align: left; border-bottom: 1px solid #333; }
    th { background: #0d0d0d; color: #4CAF50; }
    .status { padding: 15px; background: #0d0d0d; border-radius: 4px; margin: 15px 0; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚙️ BatchFlow Master</h1>
    <div class="status">
      <p><strong>Status:</strong> Ready</p>
      <p><strong>WiFi:</strong> BatchFlow-Master</p>
      <p><strong>Access:</strong> http://192.168.4.1/</p>
    </div>
    <h2>Product Setup</h2>
    <input type="text" id="prod-name" placeholder="Product Name" />
    <input type="number" id="prod-pulses" placeholder="Pulses per Liter" value="1000" />
    <button onclick="addProduct()">Add Product</button>
    <table>
      <tr><th>Name</th><th>Pulses / Liter</th><th>Actions</th></tr>
      <tbody id="prod-list"></tbody>
    </table>
    <script>
      function addProduct() {
        const name = document.getElementById('prod-name').value;
        const ppl = document.getElementById('prod-pulses').value;
        if (!name || !ppl) return alert('Fill in all fields');
        
        fetch('/api/products', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name, ppl: parseFloat(ppl) })
        }).then(() => location.reload());
      }
      
      function deleteProduct(idx) {
        fetch('/api/products/' + idx, { method: 'DELETE' })
          .then(() => location.reload());
      }
      
      fetch('/api/products')
        .then(r => r.json())
        .then(data => {
          const html = data.products.map((p, i) => 
            '<tr><td>' + p.name + '</td><td>' + p.ppl + '</td><td><button onclick="deleteProduct(' + i + ')">Delete</button></td></tr>'
          ).join('');
          document.getElementById('prod-list').innerHTML = html;
        });
    </script>
  </div>
</body>
</html>
)html";

void handleRoot() {
  if (LittleFS.exists("/index.html")) {
    File file = LittleFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
      return;
    }
  }
  server.send(200, "text/html", "<h1>BatchFlow Master</h1><p>LittleFS files not found. Check upload.</p>");
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
  server.send(404, "text/plain", "File not found");
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["state"] = "OK";
  doc["uptime"] = millis() / 1000;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
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
  
  products[productCount].name = doc["name"].as<String>();
  products[productCount].ppl = doc["ppl"] | 1000.0;
  products[productCount].ppg = products[productCount].ppl / 3.785;
  products[productCount].closeTime = 3;
  
  productCount++;
  server.send(200, "text/plain", "OK");
}

void handleProductDelete() {
  String indexStr = server.uri();
  indexStr = indexStr.substring(indexStr.lastIndexOf('/') + 1);
  int idx = indexStr.toInt();
  
  if (idx < 0 || idx >= productCount) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  
  for (int i = idx; i < productCount - 1; i++) {
    products[i] = products[i + 1];
  }
  productCount--;
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n========================================");
  Serial.println("   BATCH FLOW MASTER - STARTUP");
  Serial.println("========================================\n");
  
  // Initialize LittleFS
  Serial.println("[1/6] Initializing LittleFS...");
  if (!LittleFS.begin(true)) {
    Serial.println("      ❌ LittleFS FAILED");
  } else {
    Serial.println("      ✅ LittleFS initialized");
    
    // List files
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    Serial.println("      Files on LittleFS:");
    while (file) {
      Serial.printf("        - %s (%d bytes)\n", file.name(), file.size());
      file = root.openNextFile();
    }
  }
  delay(500);
  
  // Disable Bluetooth to save memory
  Serial.println("[2/6] Disabling Bluetooth...");
  btStop();
  Serial.println("      ✅ Done\n");
  
  // Disable WiFi sleep
  Serial.println("[3/6] Disabling WiFi sleep mode...");
  WiFi.setSleep(false);
  delay(500);
  Serial.println("      ✅ Done\n");

  // Start WiFi in AP mode
  Serial.println("[4/6] Setting WiFi mode to AP...");
  WiFi.mode(WIFI_AP);
  delay(500);
  Serial.println("      ✅ Done\n");
  
  Serial.println("[5/6] Starting AP (BatchFlow-Master)...");
  bool apOk = WiFi.softAP("BatchFlow-Master", "batchflow123", 6, false, 4);
  Serial.printf("      Result: %s\n", apOk ? "✅ SUCCESS" : "❌ FAILED");
  Serial.printf("      IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("      Mask: %s\n", WiFi.softAPSubnetMask().toString().c_str());
  delay(1000);
  Serial.println();

  // Setup web server routes
  Serial.println("[6/6] Starting Web Server on port 80...");
  server.on("/", HTTP_GET, handleRoot);
  server.on("/app.js", HTTP_GET, handleStaticFile);
  server.on("/dashboard.js", HTTP_GET, handleStaticFile);
  server.on("/style.css", HTTP_GET, handleStaticFile);
  server.on("/api/products", HTTP_GET, handleProductsList);
  server.on("/api/products", HTTP_POST, handleProductAdd);
  server.onNotFound(handleStaticFile);
  server.on("/api/status", HTTP_GET, handleStatus);
  
  server.begin();
  Serial.println("      ✅ Server started\n");
  
  Serial.println("========================================");
  Serial.println("    ✅ READY FOR CONNECTIONS");
  Serial.println("========================================");
  Serial.println("\n📡 WiFi Network: BatchFlow-Master");
  Serial.println("🔐 Password: batchflow123");
  Serial.println("🌐 Access: http://192.168.4.1/");
  Serial.println("========================================\n");
}

void loop() {
  server.handleClient();
  
  // Print station count every 5 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    int num = WiFi.softAPgetStationNum();
    Serial.printf("[%lu] Connected Stations: %d\n", millis()/1000, num);
  }
  
  delay(1);
}
