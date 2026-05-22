#pragma once

// ── WiFi AP ──────────────────────────────────────────
#define AP_SSID       "BatchLoader"
#define AP_PASSWORD   "batch1234"

// ── RS-485 Bus (WROOM master to C3 slaves) ───────────
#define RS485_TX      17          // WROOM → MAX485 DI
#define RS485_RX      16          // MAX485 RO → WROOM
#define RS485_DE      4           // MAX485 DE+RE direction pin
#define RS485_BAUD    9600
#define RS485_TIMEOUT_MS  100     // response timeout per request

// ── Slave Addresses ──────────────────────────────────
// Each ESP32-C3 node controls 1 flowmeter + 1 valve
#define SLAVE1_ADDR   1           // Line 1 (C3 slave #1)
#define SLAVE2_ADDR   2           // Line 2 (C3 slave #2)

// ── Defaults ─────────────────────────────────────────
#define DEFAULT_PULSES_PER_LITRE  450.0f
#define DEFAULT_CLOSE_TIME_MS     500      // valve closing delay
#define MAX_PRODUCTS              20
#define MAX_QUEUE                 10       // max queued loads per line
#define DB_PATH                   "/products.json"
#define SETTINGS_PATH             "/settings.json"

// ── Unit conversion ──────────────────────────────────
#define LITRES_PER_GALLON         3.78541f
