#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

// ── Product record ───────────────────────────────────
struct Product {
    char     name[32];
    float    targetLitres;
    float    calibration;    // pulses-per-litre for the single flowmeter
    uint32_t closeTime;      // ms before target to close valve
    int      valve;          // which valve to use (1 or 2)
};

// ── Simple JSON-backed product database on LittleFS ──
class Database {
public:
    Product  products[MAX_PRODUCTS];
    int      count = 0;

    void begin() {
        load();
    }

    // ── Load from flash ──────────────────────────────
    void load() {
        File f = LittleFS.open(DB_PATH, "r");
        if (!f) { 
            count = 0; 
            createDefaultProducts();
            return; 
        }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (err) { 
            count = 0; 
            createDefaultProducts();
            return; 
        }

        JsonArray arr = doc["products"].as<JsonArray>();
        count = 0;
        for (JsonObject o : arr) {
            if (count >= MAX_PRODUCTS) break;
            Product &p = products[count++];
            strlcpy(p.name, o["name"] | "Unnamed", sizeof(p.name));
            p.targetLitres = o["target"] | 1.0f;
            p.calibration = o["calibration"] | DEFAULT_PULSES_PER_LITRE;
            p.closeTime = o["closeTime"] | (uint32_t)DEFAULT_CLOSE_TIME_MS;
            p.valve = o["valve"] | 1;
        }
    }

    // ── Save to flash ────────────────────────────────
    void save() {
        JsonDocument doc;
        JsonArray arr = doc["products"].to<JsonArray>();
        
        for (int i = 0; i < count; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["name"]        = products[i].name;
            o["target"]      = products[i].targetLitres;
            o["calibration"] = products[i].calibration;
            o["closeTime"]   = products[i].closeTime;
            o["valve"]       = products[i].valve;
        }

        File f = LittleFS.open(DB_PATH, "w");
        if (f) {
            serializeJson(doc, f);
            f.close();
        }
    }

    // ── CRUD operations ──────────────────────────────
    bool addProduct(const Product &p) {
        if (count >= MAX_PRODUCTS) return false;
        products[count++] = p;
        save();
        return true;
    }

    bool updateProduct(int id, const Product &p) {
        if (id < 0 || id >= count) return false;
        products[id] = p;
        save();
        return true;
    }

    bool deleteProduct(int id) {
        if (id < 0 || id >= count) return false;
        for (int i = id; i < count - 1; i++) {
            products[i] = products[i + 1];
        }
        count--;
        save();
        return true;
    }

    Product* getProduct(int id) {
        if (id < 0 || id >= count) return nullptr;
        return &products[id];
    }

    // ── JSON serialization ───────────────────────────
    String toJson() const {
        JsonDocument doc;
        JsonArray arr = doc["products"].to<JsonArray>();
        
        for (int i = 0; i < count; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["id"]          = i;
            o["name"]        = products[i].name;
            o["target"]      = products[i].targetLitres;
            o["calibration"] = products[i].calibration;
            o["closeTime"]   = products[i].closeTime;
            o["valve"]       = products[i].valve;
        }

        String result;
        serializeJson(doc, result);
        return result;
    }

private:
    void createDefaultProducts() {
        // Create some default products
        Product p1;
        strcpy(p1.name, "Product A");
        p1.targetLitres = 1.0f;
        p1.calibration = DEFAULT_PULSES_PER_LITRE;
        p1.closeTime = DEFAULT_CLOSE_TIME_MS;
        p1.valve = 1;

        Product p2;
        strcpy(p2.name, "Product B");
        p2.targetLitres = 5.0f;
        p2.calibration = DEFAULT_PULSES_PER_LITRE;
        p2.closeTime = DEFAULT_CLOSE_TIME_MS;
        p2.valve = 2;

        addProduct(p1);
        addProduct(p2);
    }
};