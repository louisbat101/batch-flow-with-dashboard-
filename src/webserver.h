#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "database.h"
#include "settings.h"
#include "batching.h"

// ── References set from main.cpp ─────────────────────
extern Database  db;
extern Settings  settings;
extern Batching  batch;

AsyncWebServer server(80);

// ── Global scan result (scan runs in background task) ──
static volatile bool     scanRunning = false;
static volatile bool     scanDone    = false;
static String            scanResultJson;

namespace WebServer {

void init() {
    // NOTE: serveStatic must be registered AFTER all API routes,
    // otherwise it intercepts /api/* requests as file lookups.

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  GET /api/status  → live batching status
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", batch.toJson());
    });

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  GET /api/products  → product list
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/products", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", db.toJson());
    });

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/products  → add product
    //  Body: { name, cal1, cal2, ct1, ct2 }
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/products", HTTP_POST,
        [](AsyncWebServerRequest *req) {},      // no query params
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            Product p;
            strlcpy(p.name, doc["name"] | "New", sizeof(p.name));
            p.calibration1 = doc["cal1"] | DEFAULT_PULSES_PER_LITRE;
            p.calibration2 = doc["cal2"] | DEFAULT_PULSES_PER_LITRE;
            p.closeTime1   = doc["ct1"]  | (uint32_t)DEFAULT_CLOSE_TIME_MS;
            p.closeTime2   = doc["ct2"]  | (uint32_t)DEFAULT_CLOSE_TIME_MS;
            if (db.addProduct(p))
                req->send(200, "application/json", "{\"ok\":true}");
            else
                req->send(500, "application/json", "{\"error\":\"db full\"}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  PUT /api/products?id=N  → update product
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/products", HTTP_PUT,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            int id = req->hasParam("id") ? req->getParam("id")->value().toInt() : -1;
            JsonDocument doc;
            if (id < 0 || deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad request\"}");
                return;
            }
            Product p;
            strlcpy(p.name, doc["name"] | "Unnamed", sizeof(p.name));
            p.calibration1 = doc["cal1"] | DEFAULT_PULSES_PER_LITRE;
            p.calibration2 = doc["cal2"] | DEFAULT_PULSES_PER_LITRE;
            p.closeTime1   = doc["ct1"]  | (uint32_t)DEFAULT_CLOSE_TIME_MS;
            p.closeTime2   = doc["ct2"]  | (uint32_t)DEFAULT_CLOSE_TIME_MS;
            if (db.updateProduct(id, p))
                req->send(200, "application/json", "{\"ok\":true}");
            else
                req->send(404, "application/json", "{\"error\":\"not found\"}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  DELETE /api/products?id=N
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/products", HTTP_DELETE, [](AsyncWebServerRequest *req) {
        int id = req->hasParam("id") ? req->getParam("id")->value().toInt() : -1;
        if (db.deleteProduct(id))
            req->send(200, "application/json", "{\"ok\":true}");
        else
            req->send(404, "application/json", "{\"error\":\"not found\"}");
    });

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/batch/queue  { line, productId, litres }
    //  Adds a load to a line's queue
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/batch/queue", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            int   ln  = doc["line"]      | 0;
            int   pid = doc["productId"] | -1;
            float lit = doc["litres"]    | 0.0f;
            if (ln < 1 || ln > 2) {
                req->send(400, "application/json", "{\"error\":\"line must be 1 or 2\"}");
                return;
            }
            if (batch.enqueue(ln, pid, lit))
                req->send(200, "application/json", "{\"ok\":true}");
            else
                req->send(400, "application/json", "{\"error\":\"queue full or invalid\"}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/batch/start  { line }
    //  Starts the next queued load on a line
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/batch/start", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            int ln = doc["line"] | 0;
            if (ln < 1 || ln > 2) {
                req->send(400, "application/json", "{\"error\":\"line must be 1 or 2\"}");
                return;
            }
            if (batch.startNext(ln))
                req->send(200, "application/json", "{\"ok\":true}");
            else
                req->send(400, "application/json", "{\"error\":\"cannot start – queue empty or already running\"}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/batch/stop  { line }  (or no body = stop all)
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/batch/stop", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (len > 0 && !deserializeJson(doc, data, len)) {
                int ln = doc["line"] | 0;
                if (ln >= 1 && ln <= 2) {
                    batch.stop(ln);
                } else {
                    batch.stopAll();
                }
            } else {
                batch.stopAll();
            }
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/batch/queue/remove  { line, index }
    //  Removes one item from a line's queue
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/batch/queue/remove", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            int ln  = doc["line"]  | 0;
            int idx = doc["index"] | -1;
            if (ln < 1 || ln > 2) {
                req->send(400, "application/json", "{\"error\":\"line must be 1 or 2\"}");
                return;
            }
            if (batch.removeFromQueue(ln, idx))
                req->send(200, "application/json", "{\"ok\":true}");
            else
                req->send(400, "application/json", "{\"error\":\"invalid index\"}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/batch/queue/clear  { line }
    //  Clears all queued loads for a line
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/batch/queue/clear", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (len > 0) deserializeJson(doc, data, len);
            int ln = doc["line"] | 0;
            if (ln >= 1 && ln <= 2) {
                batch.clearQueue(ln);
            }
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  GET /api/settings  → unit preferences
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", settings.toJson());
    });

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  PUT /api/settings  { unit, autoStartLine2 }
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/settings", HTTP_PUT,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            bool changed = false;
            if (doc["unit"].is<const char*>()) {
                const char *u = doc["unit"] | "litres";
                if (strcmp(u, "litres") == 0 || strcmp(u, "gallons") == 0) {
                    strlcpy(settings.unit, u, sizeof(settings.unit));
                    changed = true;
                }
            }
            if (doc["autoStartLine2"].is<bool>()) {
                settings.autoStartLine2 = doc["autoStartLine2"] | false;
                changed = true;
            }
            if (doc["valve1Addr"].is<int>()) {
                int v = doc["valve1Addr"] | 11;
                if (v >= 1 && v <= 247) { settings.valve1Addr = (uint8_t)v; changed = true; }
            }
            if (doc["valve2Addr"].is<int>()) {
                int v = doc["valve2Addr"] | 12;
                if (v >= 1 && v <= 247) { settings.valve2Addr = (uint8_t)v; changed = true; }
            }
            if (changed) settings.save();
            req->send(200, "application/json", settings.toJson());
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  POST /api/valve/test  { valve: 1|2, action: "open"|"close" }
    //  Sends a Modbus RTU open/close to the configured valve address
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/valve/test", HTTP_POST,
        [](AsyncWebServerRequest *req) {},
        NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
            Serial.printf("[VALVE TEST] body received, len=%d\n", len);
            JsonDocument doc;
            if (deserializeJson(doc, data, len)) {
                Serial.println("[VALVE TEST] bad json");
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            int valve = doc["valve"] | 0;
            const char *action = doc["action"] | "";
            Serial.printf("[VALVE TEST] valve=%d action=%s\n", valve, action);
            if (valve < 1 || valve > 2) {
                req->send(400, "application/json", "{\"error\":\"valve must be 1 or 2\"}");
                return;
            }
            uint8_t addr = (valve == 1) ? settings.valve1Addr : settings.valve2Addr;
            Serial.printf("[VALVE TEST] Modbus addr=%d\n", addr);
            bool ok = false;
            if (strcmp(action, "open") == 0) {
                ok = RS485Master::modbusOpenValve(addr);
            } else if (strcmp(action, "close") == 0) {
                ok = RS485Master::modbusCloseValve(addr);
            } else {
                req->send(400, "application/json", "{\"error\":\"action must be open or close\"}");
                return;
            }
            Serial.printf("[VALVE TEST] result=%s\n", ok ? "OK" : "FAIL");
            if (ok)
                req->send(200, "application/json", "{\"ok\":true}");
            else
                req->send(500, "application/json", "{\"error\":\"no response from valve\"}");
        }
    );

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  GET /api/valve/diag?addr=1
    //  Read all 4 TONHE registers and report valve state
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/valve/diag", HTTP_GET, [](AsyncWebServerRequest *req) {
        uint8_t addr = 1;
        if (req->hasParam("addr")) addr = req->getParam("addr")->value().toInt();

        Serial.printf("\n[DIAG] Reading valve addr=%d regs 0-3\n", addr);

        JsonDocument doc;
        doc["addr"] = addr;

        // Read all 4 registers
        const char* names[] = {"deviceAddr", "positionSet", "errorStatus", "positionFeedback"};
        for (uint16_t r = 0; r <= 3; r++) {
            uint16_t val = 0;
            bool ok = RS485Master::modbusReadRegister(addr, r, &val);
            if (ok) {
                int16_t sval = (int16_t)val;
                doc[names[r]] = sval;
                Serial.printf("[DIAG] reg 0x%04X (%s) = %d (0x%04X)\n", r, names[r], sval, val);
                if (r == 1 || r == 3) {
                    // Show as degrees
                    if (sval == -32768)
                        Serial.printf("       → NO ACTION (power-on default)\n");
                    else
                        Serial.printf("       → %.2f degrees\n", sval / 100.0f);
                }
            } else {
                doc[names[r]] = "FAIL";
                Serial.printf("[DIAG] reg 0x%04X (%s) = NO RESPONSE\n", r, names[r]);
            }
        }

        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    //  API:  GET /api/valve/scan
    //  Scans Modbus addresses 1-20, tries BOTH 8N1 and 8E1
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    server.on("/api/valve/scan", HTTP_GET, [](AsyncWebServerRequest *req) {
        // If scan already running, tell client to poll
        if (scanRunning) {
            req->send(200, "application/json", "{\"status\":\"running\"}");
            return;
        }
        // If scan done, return results and reset
        if (scanDone) {
            req->send(200, "application/json", scanResultJson);
            scanDone = false;
            return;
        }
        // Start a new scan in background task (avoids watchdog + crash)
        scanRunning = true;
        scanDone = false;
        Serial.println("[VALVE SCAN] Starting scan addr 1-20, trying 8N1 then 8E1 ...");
        req->send(200, "application/json", "{\"status\":\"started\"}");

        xTaskCreatePinnedToCore([](void *) {
            JsonDocument doc;
            JsonArray arr = doc["found"].to<JsonArray>();

            // ── Pass 1: 8N1 ──
            Serial.println("[VALVE SCAN] === Pass 1: 8N1 ===");
            RS485Master::setParity8N1();
            for (uint8_t a = 1; a <= 20; a++) {
                uint16_t val = 0;
                if (RS485Master::modbusReadRegister(a, 0x0001, &val)) {
                    JsonObject obj = arr.add<JsonObject>();
                    obj["addr"] = a;
                    obj["regValue"] = val;
                    obj["parity"] = "8N1";
                    Serial.printf("[VALVE SCAN] Found addr %d (8N1)\n", a);
                }
                vTaskDelay(1);
            }

            // ── Pass 2: 8E1 ──
            Serial.println("[VALVE SCAN] === Pass 2: 8E1 ===");
            RS485Master::setParity8E1();
            for (uint8_t a = 1; a <= 20; a++) {
                uint16_t val = 0;
                if (RS485Master::modbusReadRegister(a, 0x0001, &val)) {
                    JsonObject obj = arr.add<JsonObject>();
                    obj["addr"] = a;
                    obj["regValue"] = val;
                    obj["parity"] = "8E1";
                    Serial.printf("[VALVE SCAN] Found addr %d (8E1)\n", a);
                }
                vTaskDelay(1);
            }

            RS485Master::setParity8N1();
            doc["count"] = arr.size();
            doc["status"] = "done";
            Serial.printf("[VALVE SCAN] Done – %d found\n", arr.size());
            serializeJson(doc, scanResultJson);
            scanRunning = false;
            scanDone = true;
            vTaskDelete(NULL);
        }, "scanTask", 8192, NULL, 1, NULL, 0);
    });

    // ── CORS headers for Android WebView ─────────────
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    // ── Serve the single-page web app ────────────────
    // MUST be after all API routes so /api/* is not intercepted
    server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");

    server.begin();
}

} // namespace WebServer
