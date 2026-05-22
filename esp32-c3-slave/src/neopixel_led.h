#pragma once

#include <Adafruit_NeoPixel.h>
#include "slave_config.h"

// ═══════════════════════════════════════════════════════════════════════
// WS2811/NeoPixel Status LED Controller
// Single 5V RGB LED for system status indication
// ═══════════════════════════════════════════════════════════════════════

class NeoPixelLED {
private:
  Adafruit_NeoPixel pixels;
  uint32_t currentColor;
  uint32_t blinkColor;
  unsigned long blinkMillis;
  bool blinking;
  bool blinkState;

public:
  // LED Status Colors
  static constexpr uint32_t COLOR_BOOT       = 0xFFFFFF;  // White - Booting
  static constexpr uint32_t COLOR_MASTER     = 0x0000FF;  // Blue - Master board
  static constexpr uint32_t COLOR_IDLE       = 0x00FFFF;  // Cyan - Slave idle/connected
  static constexpr uint32_t COLOR_DISPENSING = 0x00FF00;  // Green - Dispensing
  static constexpr uint32_t COLOR_WAITING    = 0xFFFF00;  // Yellow - Waiting
  static constexpr uint32_t COLOR_FAULT      = 0xFF0000;  // Red - Fault/Error
  static constexpr uint32_t COLOR_NO_FLOW    = 0xFF8000;  // Orange - No flow detected
  static constexpr uint32_t COLOR_OFF        = 0x000000;  // Black - Off

  NeoPixelLED() : pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
                  currentColor(COLOR_OFF), blinking(false), blinkState(false) {}

  void begin() {
    pixels.begin();
    setColor(COLOR_BOOT);
    Serial.println("[LED] NeoPixel initialized on GPIO " + String(NEOPIXEL_PIN));
  }

  // Set solid color (no blink)
  void setColor(uint32_t color) {
    currentColor = color;
    blinking = false;
    pixels.setPixelColor(0, color);
    pixels.show();
    Serial.printf("[LED] Color set to 0x%06X\n", color);
  }

  // Set blinking color with interval in milliseconds
  void setBlinking(uint32_t color, unsigned long intervalMs) {
    blinkColor = color;
    blinkMillis = intervalMs;
    blinking = true;
    blinkState = false;
    Serial.printf("[LED] Blinking 0x%06X at %lu ms\n", color, intervalMs);
  }

  // Update LED state (call in loop)
  void update() {
    if (!blinking) return;

    static unsigned long lastToggle = 0;
    unsigned long now = millis();

    if (now - lastToggle >= blinkMillis) {
      lastToggle = now;
      blinkState = !blinkState;

      if (blinkState) {
        pixels.setPixelColor(0, blinkColor);
      } else {
        pixels.setPixelColor(0, COLOR_OFF);
      }
      pixels.show();
    }
  }

  // Get current color
  uint32_t getColor() const {
    return currentColor;
  }

  // Check if currently blinking
  bool isBlinking() const {
    return blinking;
  }
};

// Global LED instance
extern NeoPixelLED statusLED;
