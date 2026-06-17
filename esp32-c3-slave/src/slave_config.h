// ═══════════════════════════════════════════════════════════════════════
// ESP32-C3 Super Mini + ESC3E05 4-Channel Relay Expansion Board
// Pinout reference: CH1-CH4(Relay DO) → GPIO 5/6/7/10
//                   RS485 on GPIO 20/21/9 (RXD/TXD/RD)
// ═══════════════════════════════════════════════════════════════════════







// ── Address Selection via Jumpers to GND ─────────────
// Each C3 board reads GPIO 1/2/3 on boot to set its address.
// No recompiling needed — just jumper the pin(s) to GND.
//
//   GPIO1 | GPIO2 | GPIO3 | Address
//   ──────┼───────┼───────┼────────
//   open  | open  | open  | 1 (default - no jumpers)
//   open  | open  | GND   | 2
//   open  | GND   | open  | 3
//   open  | GND   | GND   | 4
//   GND   | open  | open  | 5
//   GND   | open  | GND   | 6
//   GND   | GND   | open  | 7
//   GND   | GND   | GND   | 8
#define ADDR_JMP_0        2       // LSB - jumper to GND = bit 0
#define ADDR_JMP_1        3       // bit 1
#define ADDR_JMP_2        1       // bit 2 (MSB)








// ── RS-485 Pins (ESC3E05 Expansion Board with built-in RS-485) ──────
// CORRECT pins for ESC3E05 expansion board:
// GPIO 20 = RXD (receive RS-485 data)
// GPIO 21 = TXD (transmit RS-485 data)
// GPIO 9  = RD  (DE/RE direction control)
// NOTE: GPIO 0 is bootstrap pin, CANNOT use for UART!
#define RS485_RXD_PIN     20      // GPIO 20 (RX) - CORRECT for ESC3E05
#define RS485_TXD_PIN     21      // GPIO 21 (TX) - CORRECT for ESC3E05
#define RS485_RD_PIN      9       // Direction control (DE/RE)
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










// ── RS-485 Activity LED ─────────────────────────────
// Blinks whenever data is sent or received on RS-485.
// Uses GPIO 4 (NeoPixel pin repurposed).
#define LED_485_PIN       4       // GPIO 4 - RS-485 activity LED
#define LED_485_FLASH_MS  100     // how long LED stays on per message
