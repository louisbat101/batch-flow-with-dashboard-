#pragma once

// ═══════════════════════════════════════════════════════════════════════
// ESP32-C3 Super Mini Master Controller
// RS-485 Modbus RTU + WiFi + Product Database
// ═══════════════════════════════════════════════════════════════════════

// ── WiFi Configuration ───────────────────────────────────
// Master can connect to WiFi OR run as AP
// Set WIFI_SSID to empty ("") to skip WiFi connection and just use AP
#define WIFI_SSID        ""           // Leave empty to skip WiFi connection
#define WIFI_PASSWORD    ""           // Leave empty to skip WiFi connection
#define WIFI_AP_SSID     "BatchFlow-Master"
#define WIFI_AP_PASS     "batchflow123"
#define WIFI_AP_PORT     80

// ── RS-485 Pins (ESP32-C3 Super Mini - MASTER CUSTOM BUILD) ──────
// Master uses YOUR wiring: GPIO9=RX, GPIO20=RD, GPIO21=TX
#define RS485_RXD_PIN     9       // RXD from MAX485 (custom wiring)
#define RS485_TXD_PIN     21      // TXD to MAX485
#define RS485_RD_PIN      20      // RD/RO direction control (custom wiring)
#define RS485_BAUD        9600    // Modbus RTU standard

// ── WS2811/NeoPixel Status LED ──────────────────────────
#define NEOPIXEL_PIN      4       // GPIO 4 (WS2811 data line, 5V)
#define NEOPIXEL_COUNT    1       // Single LED

// ── Slave Configuration ──────────────────────────────────
#define MIN_SLAVE_ADDR    1       // Slave 1
#define MAX_SLAVE_ADDR    10      // Up to 10 slaves
#define SLAVE_TIMEOUT_MS  1000    // Timeout waiting for slave response

// ── Storage (LittleFS) ───────────────────────────────────
// (File paths defined in database.h)
#define MAX_PRODUCTS      50
#define MAX_PRODUCT_NAME  32

// ── Units ────────────────────────────────────────────────
#define UNIT_LITERS       0
#define UNIT_GALLONS      1

// ── Modbus RTU Function Codes ────────────────────────────
#define MODBUS_READ_COILS           0x01
#define MODBUS_WRITE_SINGLE_COIL    0x05
#define MODBUS_WRITE_MULTIPLE_COILS 0x0F
#define MODBUS_READ_HOLDING_REG     0x03
#define MODBUS_WRITE_HOLDING_REG    0x06

// ── Slave Command Addresses (Modbus Registers) ──────────
#define SLAVE_REG_VALVE_CONTROL     0x0000  // 0x05 = open valve, 0x00 = close
#define SLAVE_REG_FLOW_COUNT        0x0001  // Read current pulse count
#define SLAVE_REG_STATUS            0x0002  // Read slave status byte
#define SLAVE_REG_PPL_CONFIG        0x0003  // Pulses per liter (write)

// ── Web Server Endpoints ─────────────────────────────────
// GET  / → Dashboard UI
// POST /api/products → Create product
// GET  /api/products → List all products
// PUT  /api/products/:id → Update product
// DELETE /api/products/:id → Delete product
// POST /api/load → Create load (product + amount per station)
// POST /api/run/start → Start dispensing sequence
// POST /api/run/stop → Stop dispensing
// GET  /api/status → Real-time status (JSON)
