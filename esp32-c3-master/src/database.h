#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// Product structure
struct Product {
  uint16_t id;
  char name[32];
  float pulsesPerLiter;
  float pulsesPerGallon;
  uint16_t valveClosingTimeMs;  // Time to close valve after stopping pump
  uint8_t stationColor;         // RGB color code (0-7)
};

// Load/Batch structure (per-station assignment)
struct LoadStation {
  uint8_t stationAddr;          // Slave address 1-10
  uint16_t productId;
  float targetAmount;           // Liters or Gallons
  uint8_t unit;                 // UNIT_LITERS or UNIT_GALLONS
};

class Database {
public:
  static constexpr const char* PRODUCTS_FILE = "/products.json";
  static constexpr const char* SETTINGS_FILE = "/settings.json";

  // Initialize LittleFS
  static bool begin() {
    if (!LittleFS.begin()) {
      Serial.println("[DB] LittleFS mount failed!");
      return false;
    }
    Serial.println("[DB] LittleFS mounted");
    return true;
  }

  // Create product
  static bool createProduct(const Product& prod) {
    DynamicJsonDocument doc(8192);
    
    if (LittleFS.exists(PRODUCTS_FILE)) {
      File f = LittleFS.open(PRODUCTS_FILE, "r");
      deserializeJson(doc, f);
      f.close();
    }

    JsonArray products = doc.as<JsonArray>();
    if (!products.isNull()) {
      JsonObject newProd = products.createNestedObject();
      newProd["id"] = prod.id;
      newProd["name"] = prod.name;
      newProd["ppl"] = prod.pulsesPerLiter;
      newProd["ppg"] = prod.pulsesPerGallon;
      newProd["closeTimeMs"] = prod.valveClosingTimeMs;
      newProd["color"] = prod.stationColor;
    }

    File f = LittleFS.open(PRODUCTS_FILE, "w");
    serializeJson(doc, f);
    f.close();
    Serial.printf("[DB] Product created: %s (ID=%u)\n", prod.name, prod.id);
    return true;
  }

  // Read all products as JSON string
  static String getAllProductsJSON() {
    if (!LittleFS.exists(PRODUCTS_FILE)) {
      return "[]";
    }
    
    File f = LittleFS.open(PRODUCTS_FILE, "r");
    String content;
    while (f.available()) {
      content += (char)f.read();
    }
    f.close();
    return content;
  }

  // Get product by ID
  static bool getProduct(uint16_t id, Product& prod) {
    if (!LittleFS.exists(PRODUCTS_FILE)) return false;

    File f = LittleFS.open(PRODUCTS_FILE, "r");
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, f);
    f.close();

    JsonArray products = doc.as<JsonArray>();
    for (JsonObject p : products) {
      if (p["id"] == id) {
        prod.id = p["id"];
        strlcpy(prod.name, p["name"] | "", sizeof(prod.name));
        prod.pulsesPerLiter = p["ppl"] | 0.0;
        prod.pulsesPerGallon = p["ppg"] | 0.0;
        prod.valveClosingTimeMs = p["closeTimeMs"] | 100;
        prod.stationColor = p["color"] | 0;
        return true;
      }
    }
    return false;
  }

  // Delete product by ID
  static bool deleteProduct(uint16_t id) {
    if (!LittleFS.exists(PRODUCTS_FILE)) return false;

    File f = LittleFS.open(PRODUCTS_FILE, "r");
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, f);
    f.close();

    JsonArray products = doc.as<JsonArray>();
    for (size_t i = 0; i < products.size(); i++) {
      if (products[i]["id"] == id) {
        products.remove(i);
        break;
      }
    }

    f = LittleFS.open(PRODUCTS_FILE, "w");
    serializeJson(doc, f);
    f.close();
    Serial.printf("[DB] Product deleted: ID=%u\n", id);
    return true;
  }

  // Save settings (unit preference, etc)
  static bool saveSettings(const String& json) {
    File f = LittleFS.open(SETTINGS_FILE, "w");
    f.print(json);
    f.close();
    Serial.println("[DB] Settings saved");
    return true;
  }

  // Load settings
  static String getSettings() {
    if (!LittleFS.exists(SETTINGS_FILE)) {
      return "{\"unit\": 0}";  // Default to liters
    }
    File f = LittleFS.open(SETTINGS_FILE, "r");
    String content;
    while (f.available()) {
      content += (char)f.read();
    }
    f.close();
    return content;
  }
};
