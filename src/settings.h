#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

// ── System settings stored in LittleFS ───────────────
class Settings {
public:
    // "litres" or "gallons"
    char unit[16];
    bool autoStartLine2;   // auto-start line 2 when line 1 finishes
    uint8_t valve1Addr;    // Modbus RTU address for valve 1 (1-247)
    uint8_t valve2Addr;    // Modbus RTU address for valve 2 (1-247)

    void begin() {
        load();
    }

    void load() {
        strlcpy(unit, "litres", sizeof(unit));  // default
        autoStartLine2 = false;
        valve1Addr = 11;   // default Modbus address for valve 1
        valve2Addr = 12;   // default Modbus address for valve 2

        File f = LittleFS.open(SETTINGS_PATH, "r");
        if (!f) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, f);
        f.close();
        if (err) return;

        strlcpy(unit, doc["unit"] | "litres", sizeof(unit));
        autoStartLine2 = doc["autoStartLine2"] | false;
        valve1Addr = doc["valve1Addr"] | 11;
        valve2Addr = doc["valve2Addr"] | 12;
    }

    void save() {
        JsonDocument doc;
        doc["unit"] = unit;
        doc["autoStartLine2"] = autoStartLine2;
        doc["valve1Addr"] = valve1Addr;
        doc["valve2Addr"] = valve2Addr;

        File f = LittleFS.open(SETTINGS_PATH, "w");
        if (f) { serializeJson(doc, f); f.close(); }
    }

    bool isGallons() {
        return strcmp(unit, "gallons") == 0;
    }

    // Convert litres (internal) → display unit
    float toDisplay(float litres) {
        if (isGallons()) return litres / LITRES_PER_GALLON;
        return litres;
    }

    // Convert display unit → litres (internal)
    float fromDisplay(float value) {
        if (isGallons()) return value * LITRES_PER_GALLON;
        return value;
    }

    String unitLabel() {
        if (isGallons()) return "gal";
        return "L";
    }

    String toJson() {
        JsonDocument doc;
        doc["unit"] = unit;
        doc["unitLabel"] = unitLabel();
        doc["conversionFactor"] = isGallons() ? (1.0f / LITRES_PER_GALLON) : 1.0f;
        doc["autoStartLine2"] = autoStartLine2;
        doc["valve1Addr"] = valve1Addr;
        doc["valve2Addr"] = valve2Addr;
        String out;
        serializeJson(doc, out);
        return out;
    }
};
