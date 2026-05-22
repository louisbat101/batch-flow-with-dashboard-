#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

// ── Product record ───────────────────────────────────
struct Product {
    char     name[32];
    float    calibration1;   // pulses-per-litre flowmeter 1
    float    calibration2;   // pulses-per-litre flowmeter 2
    uint32_t closeTime1;     // ms before target to close valve 1
    uint32_t closeTime2;     // ms before target to close valve 2
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
        if (!f) { count = 0; return; }

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (err) { count = 0; return; }

        JsonArray arr = doc["products"].as<JsonArray>();
        count = 0;
        for (JsonObject o : arr) {
            if (count >= MAX_PRODUCTS) break;
            Product &p = products[count++];
            strlcpy(p.name, o["name"] | "Unnamed", sizeof(p.name));
            p.calibration1 = o["cal1"] | DEFAULT_PULSES_PER_LITRE;
            p.calibration2 = o["cal2"] | DEFAULT_PULSES_PER_LITRE;
            p.closeTime1   = o["ct1"]  | (uint32_t)DEFAULT_CLOSE_TIME_MS;
            p.closeTime2   = o["ct2"]  | (uint32_t)DEFAULT_CLOSE_TIME_MS;
        }
    }

    // ── Save to flash ────────────────────────────────
    void save() {
        JsonDocument doc;
        JsonArray arr = doc["products"].to<JsonArray>();
        for (int i = 0; i < count; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["name"] = products[i].name;
            o["cal1"] = products[i].calibration1;
            o["cal2"] = products[i].calibration2;
            o["ct1"]  = products[i].closeTime1;
            o["ct2"]  = products[i].closeTime2;
        }
        File f = LittleFS.open(DB_PATH, "w");
        if (f) { serializeJson(doc, f); f.close(); }
    }

    // ── CRUD helpers ─────────────────────────────────
    bool addProduct(const Product &p) {
        if (count >= MAX_PRODUCTS) return false;
        products[count++] = p;
        save();
        return true;
    }

    bool updateProduct(int idx, const Product &p) {
        if (idx < 0 || idx >= count) return false;
        products[idx] = p;
        save();
        return true;
    }

    bool deleteProduct(int idx) {
        if (idx < 0 || idx >= count) return false;
        for (int i = idx; i < count - 1; i++) products[i] = products[i + 1];
        count--;
        save();
        return true;
    }

    int findByName(const char *name) {
        for (int i = 0; i < count; i++)
            if (strcmp(products[i].name, name) == 0) return i;
        return -1;
    }

    // ── Serialise full list to JSON string ───────────
    String toJson() {
        JsonDocument doc;
        JsonArray arr = doc["products"].to<JsonArray>();
        for (int i = 0; i < count; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["id"]   = i;
            o["name"] = products[i].name;
            o["cal1"] = products[i].calibration1;
            o["cal2"] = products[i].calibration2;
            o["ct1"]  = products[i].closeTime1;
            o["ct2"]  = products[i].closeTime2;
        }
        String out;
        serializeJson(doc, out);
        return out;
    }
};
