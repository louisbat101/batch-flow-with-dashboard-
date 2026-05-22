#pragma once

// ═══════════════════════════════════════════════════════════════════════
// ESP32-C3 Super Mini + ESC3E05 4-Channel Relay Expansion Board
// Pinout reference: CH1-CH4(Relay DO) → GPIO 5/6/7/10
//                   RD/RXD/TXD(RS485) → GPIO 9/20/21
// ═══════════════════════════════════════════════════════════════════════

// ── Slave Address ────────────────────────────────────
// Now configured at runtime via the config web UI.
// Stored in NVS (flash). Default if not yet set:
#define DEFAULT_SLAVE_ADDR   1
#define MIN_SLAVE_ADDR       1     // Slave 1
#define MAX_SLAVE_ADDR       10    // Slave 10

// ── Config WiFi AP ───────────────────────────────────
// On first boot (or when button held), the C3 starts a
// WiFi AP so you can pick the address from a dropdown.
#define CONFIG_AP_SSID       "FlowNode-Setup"
#define CONFIG_AP_PASS       "flownode123"
#define CONFIG_AP_PORT       80

// ── RS-485 Pins (ESP32-C3 Super Mini) ──────────────
// Using GPIO 9 for RXD, GPIO 20/21 for RD/TXD
// (Avoids USB JTAG pins 3-5 and relay channel pins 5-7,10)
#define RS485_RXD_PIN     9       // RXD from MAX485
#define RS485_TXD_PIN     21      // TXD to MAX485
#define RS485_RD_PIN      20      // RD/RO (receive disable / receive output) from MAX485
#define RS485_BAUD        9600    // Modbus RTU standard

// ── 4-Channel Relay / Valve Control Pins ────────────
// ESC3E05 Relay module CH1-CH4 (active-HIGH)
#define RELAY_CH1_PIN     5       // Relay Channel 1 (HIGH = ON)
#define RELAY_CH2_PIN     6       // Relay Channel 2 (HIGH = ON)
#define RELAY_CH3_PIN     7       // Relay Channel 3 (HIGH = ON)
#define RELAY_CH4_PIN     10      // Relay Channel 4 (HIGH = ON)

// Legacy valve aliases (for backward compatibility)
#define VALVE_1_PIN       RELAY_CH1_PIN   // Valve 1 control
#define VALVE_2_PIN       RELAY_CH2_PIN   // Valve 2 control

// ── Flowmeter Pins (if used) ────────────────────────
// GPIO 8: Flowmeter pulse input (ISR-based counting)
#define FLOWMETER_PIN     8       // GPIO 8 (pulse input for flow measurement)

// ── WS2811/NeoPixel Status LED ──────────────────────
// 5V WS2811 RGB LED on GPIO 4 (data pin)
// Displays: Blue=Master, Cyan=Slave idle, Green=Dispensing,
//           Yellow=Waiting, Red=Fault, Orange=No-flow, White=Booting
#define NEOPIXEL_PIN      4       // GPIO 4 (WS2811 data line, 5V)
#define NEOPIXEL_COUNT    1       // Single LED
#define LED_POWER_PIN     0       // Power LED (GPIO 0, currently unused)
#define LED_485_PIN       2       // RS-485 activity LED (GPIO 2, currently unused)
#define LED_485_FLASH_MS  100     // how long 485 LED stays on per message
