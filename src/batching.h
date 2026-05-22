#pragma once
#include <Arduino.h>
#include "config.h"
#include "database.h"
#include "settings.h"
#include "rs485_master.h"

enum BatchState { IDLE, QUEUED, RUNNING, DONE };

// ── One queued job ───────────────────────────────────
struct QueueItem {
    int   productIdx;
    float litres;
};

// ── One independent batch line ───────────────────────
struct BatchLine {
    BatchState state        = IDLE;
    int        productIdx   = -1;
    float      targetLitres = 0;
    float      loaded       = 0;
    bool       valveClosed  = false;

    // ── Queue ────────────────────────────────────────
    QueueItem  queue[MAX_QUEUE];
    int        queueCount   = 0;

    bool enqueue(int prodIdx, float litres) {
        if (queueCount >= MAX_QUEUE) return false;
        queue[queueCount].productIdx = prodIdx;
        queue[queueCount].litres     = litres;
        queueCount++;
        if (state == IDLE || state == DONE) state = QUEUED;
        return true;
    }

    void dequeue() {
        if (queueCount <= 0) return;
        for (int i = 0; i < queueCount - 1; i++) {
            queue[i] = queue[i + 1];
        }
        queueCount--;
    }

    void clearQueue() {
        queueCount = 0;
        if (state == QUEUED) state = IDLE;
    }

    bool removeFromQueue(int index) {
        if (index < 0 || index >= queueCount) return false;
        for (int i = index; i < queueCount - 1; i++) {
            queue[i] = queue[i + 1];
        }
        queueCount--;
        if (queueCount == 0 && state == QUEUED) state = IDLE;
        return true;
    }
};

// ── Batching engine – two independent lines ──────────
class Batching {
public:
    BatchLine line[2];
    Database *db = nullptr;
    Settings *settings = nullptr;

    void begin(Database *database, Settings *sett) {
        db = database;
        settings = sett;
    }

    // ── Add a load to a line's queue ─────────────────
    bool enqueue(int lineNum, int prodIdx, float litres) {
        if (lineNum < 1 || lineNum > 2) return false;
        if (!db || prodIdx < 0 || prodIdx >= db->count) return false;
        return line[lineNum - 1].enqueue(prodIdx, litres);
    }

    // ── Start next queued load on a line ─────────────
    bool startNext(int lineNum) {
        if (lineNum < 1 || lineNum > 2) return false;
        int idx = lineNum - 1;
        if (line[idx].state == RUNNING) return false;
        if (line[idx].queueCount <= 0) return false;
        if (!db) return false;

        QueueItem &item = line[idx].queue[0];
        if (item.productIdx < 0 || item.productIdx >= db->count) {
            line[idx].dequeue();
            return false;
        }

        line[idx].productIdx   = item.productIdx;
        line[idx].targetLitres = item.litres;
        line[idx].loaded       = 0;
        line[idx].valveClosed  = false;

        // Remove from queue
        line[idx].dequeue();

        if (lineNum == 1) {
            RS485Master::resetPulses1();
            RS485Master::modbusOpenValve(settings->valve1Addr);
        } else {
            RS485Master::resetPulses2();
            RS485Master::modbusOpenValve(settings->valve2Addr);
        }

        line[idx].state = RUNNING;
        Serial.printf("[BATCH] Line %d started: product=%s  target=%.2f L\n",
                      lineNum, db->products[line[idx].productIdx].name,
                      line[idx].targetLitres);
        return true;
    }

    // ── Call from loop() ─────────────────────────────
    void update() {
        if (!db) return;

        // ── Update Line 1 ────────────────────────────
        bool line1JustDone = false;
        if (line[0].state == RUNNING) {
            Product &p = db->products[line[0].productIdx];
            line[0].loaded = RS485Master::litres1(p.calibration1);

            float earlyClose = (float)p.closeTime1 / 1000.0f;
            if (!line[0].valveClosed && line[0].loaded >= (line[0].targetLitres - earlyClose)) {
                RS485Master::modbusCloseValve(settings->valve1Addr);
                line[0].valveClosed = true;
                Serial.printf("[BATCH] Line 1 valve closed at %.2f L\n", line[0].loaded);
            }
            if (line[0].valveClosed) {
                line[0].state = DONE;
                line1JustDone = true;
                Serial.printf("[BATCH] Line 1 done! loaded=%.2f L\n", line[0].loaded);
            }
        }

        // ── Update Line 2 ────────────────────────────
        if (line[1].state == RUNNING) {
            Product &p = db->products[line[1].productIdx];
            line[1].loaded = RS485Master::litres2(p.calibration2);

            float earlyClose = (float)p.closeTime2 / 1000.0f;
            if (!line[1].valveClosed && line[1].loaded >= (line[1].targetLitres - earlyClose)) {
                RS485Master::modbusCloseValve(settings->valve2Addr);
                line[1].valveClosed = true;
                Serial.printf("[BATCH] Line 2 valve closed at %.2f L\n", line[1].loaded);
            }
            if (line[1].valveClosed) {
                line[1].state = DONE;
                Serial.printf("[BATCH] Line 2 done! loaded=%.2f L\n", line[1].loaded);
            }
        }

        // ── Auto-start Line 2 when Line 1 finishes ──
        if (line1JustDone && settings && settings->autoStartLine2) {
            if (line[1].state != RUNNING && line[1].queueCount > 0) {
                Serial.println("[BATCH] Auto-starting Line 2");
                startNext(2);
            }
        }
    }

    // ── Stop a specific line ─────────────────────────
    void stop(int lineNum) {
        if (lineNum < 1 || lineNum > 2) return;
        int idx = lineNum - 1;
        if (lineNum == 1) RS485Master::modbusCloseValve(settings->valve1Addr);
        else              RS485Master::modbusCloseValve(settings->valve2Addr);
        line[idx].state = (line[idx].queueCount > 0) ? QUEUED : IDLE;
    }

    // ── Stop both lines ──────────────────────────────
    void stopAll() {
        stop(1);
        stop(2);
    }

    // ── Clear queue for a line ───────────────────────
    void clearQueue(int lineNum) {
        if (lineNum < 1 || lineNum > 2) return;
        line[lineNum - 1].clearQueue();
    }

    // ── Remove one item from queue ───────────────────
    bool removeFromQueue(int lineNum, int index) {
        if (lineNum < 1 || lineNum > 2) return false;
        return line[lineNum - 1].removeFromQueue(index);
    }

    // ── Progress for one line 0..100 ─────────────────
    float progress(int lineNum) const {
        if (lineNum < 1 || lineNum > 2) return 0;
        int idx = lineNum - 1;
        if (line[idx].targetLitres <= 0) return 0;
        float pct = (line[idx].loaded / line[idx].targetLitres) * 100.0f;
        return (pct > 100.0f) ? 100.0f : pct;
    }

    // ── Status JSON for the web UI ───────────────────
    String toJson() {
        JsonDocument doc;

        for (int i = 0; i < 2; i++) {
            String key = "line" + String(i + 1);
            JsonObject obj = doc[key].to<JsonObject>();

            const char *stStr = "idle";
            if (line[i].state == QUEUED)  stStr = "queued";
            if (line[i].state == RUNNING) stStr = "running";
            if (line[i].state == DONE)    stStr = "done";

            obj["state"]    = stStr;
            obj["product"]  = (line[i].productIdx >= 0 && db) ? db->products[line[i].productIdx].name : "";
            obj["target"]   = line[i].targetLitres;
            obj["loaded"]   = line[i].loaded;
            obj["progress"] = progress(i + 1);

            // Queue
            JsonArray q = obj["queue"].to<JsonArray>();
            for (int j = 0; j < line[i].queueCount; j++) {
                JsonObject qi = q.add<JsonObject>();
                qi["productId"] = line[i].queue[j].productIdx;
                qi["product"]   = (line[i].queue[j].productIdx >= 0 && db)
                                  ? db->products[line[i].queue[j].productIdx].name : "";
                qi["litres"]    = line[i].queue[j].litres;
            }
        }

        String out;
        serializeJson(doc, out);
        return out;
    }
};
