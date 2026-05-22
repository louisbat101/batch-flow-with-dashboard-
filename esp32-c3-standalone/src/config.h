#pragma once

// ── WiFi AP ──────────────────────────────────────────
#define AP_SSID       "BatchFlow-C3"
#define AP_PASSWORD   "batch1234"

// ── ESP32-C3 Pins (matching your PCB layout) ────────
#define FLOW_HALL_A_PIN  2        // Flowmeter Hall sensor A pulse input
#define FLOW_HALL_B_PIN  3        // Flowmeter Hall sensor B pulse input
#define RS485_TX_PIN     4        // C3 → MAX485 DI
#define RS485_RX_PIN     5        // MAX485 RO → C3
#define RS485_DE_RE_PIN  6        // MAX485 DE+RE control
#define LED_POWER_PIN    8        // Power LED (always on)
#define LED_STATUS_PIN   10       // Status LED (batching activity)

// ── RS485 Configuration ─────────────────────────────
#define RS485_BAUD       9600     // Baud rate for valve communication

// ── Defaults ─────────────────────────────────────────
#define DEFAULT_PULSES_PER_LITRE  450.0f
#define DEFAULT_CLOSE_TIME_MS     500      // valve closing delay before target
#define MAX_PRODUCTS              20
#define DB_PATH                   "/products.json"
#define SETTINGS_PATH             "/settings.json"

// ── Safety limits ────────────────────────────────────
#define MAX_BATCH_LITRES         100.0f    // safety limit per batch
#define MAX_BATCH_TIME_MS        300000    // 5 minutes max batch time
#define VALVE_SAFETY_TIMEOUT_MS  10000     // auto-close valves after 10s if no commands

// ── LED flash timing ─────────────────────────────────
#define LED_STATUS_FLASH_MS      200       // status LED flash duration