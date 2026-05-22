#pragma once

// ── Slave Address ────────────────────────────────────
// Now configured at runtime via the config web UI.
// Stored in NVS (flash). Default if not yet set:
#define DEFAULT_SLAVE_ADDR   1
#define MIN_SLAVE_ADDR       1     // Flow 1
#define MAX_SLAVE_ADDR       10    // Flow 10

// ── Config WiFi AP ───────────────────────────────────
// On first boot (or when button held), the C3 starts a
// WiFi AP so you can pick the address from a dropdown.
#define CONFIG_AP_SSID       "FlowNode-Setup"
#define CONFIG_AP_PASS       "flow1234"
#define CONFIG_AP_PORT       80

// ── RS-485 Pins (ESP32-C3-DevKitM-1) ────────────────
// **CRITICAL**: GPIO 4/5 are USB JTAG on DevKit - DO NOT USE for Serial1!
// Use GPIO 8/9 instead for RS-485 UART
#define RS485_TX_PIN      8       // UART1 TX → MAX485 DI
#define RS485_RX_PIN      9       // MAX485 RO → UART1 RX
#define RS485_DE_PIN      10      // MAX485 DE+RE (direction control)
#define RS485_BAUD        9600    // Modbus RTU standard

// ── Flowmeter Pins (2 pulse outputs) ─────────────────
// ESP32-C3 available: GPIO 0-3, 5, 6, 7, 19, 20, 21, 22
// GPIO 2,3 used by USB JTAG on DevKit - avoid!
// Use GPIO 5,6 for flow meters (interrupt-capable)
#define FLOW_PIN_A        5       // pulse output A (interrupt-capable)
#define FLOW_PIN_B        6       // pulse output B (interrupt-capable)

// ── Valve Control Pin ────────────────────────────────
// Simple relay / MOSFET driver – HIGH = open, LOW = close
#define VALVE_PIN         7       // digital output

// ── LEDs ─────────────────────────────────────────────
#define LED_POWER_PIN     19      // Power LED – always on
#define LED_485_PIN       20      // RS-485 activity LED – blinks on comms
#define LED_485_FLASH_MS  100     // how long 485 LED stays on per message
