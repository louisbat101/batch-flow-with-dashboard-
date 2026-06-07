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

// ── UART to Teensy (Status Link) ─────────────────────────
// Teensy Pin 1 (RX1) ← C3 GPIO 21 (TX)
// Teensy Pin 2 (TX1) ← C3 GPIO 20 (RX)
#define UART_RXD_PIN      20        // GPIO 20 - receives status from Teensy TX1
#define UART_TXD_PIN      21        // GPIO 21 - sends acks to Teensy RX1
#define UART_BAUD         115200
#define UART_BUFFER_SIZE  256

// ── RS-485 Pins (NOT USED on C3 Master - only on Slaves) ──────
// (Kept for reference only - C3 Master does NOT communicate via RS-485)

// ── WS2811/NeoPixel Status LED ──────────────────────────
#define NEOPIXEL_PIN      4       // GPIO 4 (WS2811 data line, 5V)
#define NEOPIXEL_COUNT    1       // Single LED

// ── Slave Configuration ──────────────────────────────────
#define MIN_SLAVE_ADDR    1       // Slave 1
#define MAX_SLAVE_ADDR    1       // Currently only 1 slave connected (was 10 - caused ghost slaves!)
#define SLAVE_TIMEOUT_MS  100     // Modbus timeout (was 1000ms - blocked WiFi/WebServer!)

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
