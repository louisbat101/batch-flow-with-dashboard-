# ESP32 Batch Flow Controller

What I created:

- PlatformIO project for ESP32 firmware (web UI served from SPIFFS)
- Flowmeter pulse counting with calibration and litres calculation
- Simple RS485 valve control (Modbus RTU Write Single Coil at address 0x0000)
- REST API endpoints: /api/status, /api/start, /api/stop, /api/products, /api/calibrate
- Simple web UI in SPIFFS (index.html, app.js)
- Sample products database in `data/products.json`

Build & flash (PlatformIO):

1. Install PlatformIO in VSCode or use `platformio` CLI.
2. Put the ESP32 board in flash mode and run:

```bash
platformio run -t upload
```

Notes, assumptions and next steps:

- RS485 valve control uses a simple Modbus RTU frame and writes coil 0x0000. If your valves expect different addresses or commands, update `RS485Valve::sendModbusWriteSingleCoil` accordingly.
- Flowmeter pulses-per-litre default is 450. Calibrate via the web UI by calling POST /api/calibrate with body {"meter":1,"pulses_per_litre":450}
- The ESP32 starts as an AP named `BatchFlow-ESP32` by default. You may want to change to STA mode and store WiFi credentials.
- For production, add authentication, input validation, and safety interlocks.

Android WebView app (skeleton):

Below is a minimal Android app MainActivity you can use to wrap the web UI. Place your ESP32's IP or hostname in `WEB_URL`.

MainActivity.kt:

```kotlin
package com.example.batchflow

import android.os.Bundle
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity() {
  private val WEB_URL = "http://192.168.4.1/" // replace with your ESP32 URL
  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    val wv = WebView(this)
    wv.settings.javaScriptEnabled = true
    wv.webViewClient = WebViewClient()
    wv.loadUrl(WEB_URL)
    setContentView(wv)
  }
}
```

Use Android Studio to create a new project and replace MainActivity with the snippet above. Add INTERNET permission to `AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.INTERNET" />
```

If you want, I can also generate a complete Android Studio project skeleton.
# Batch Loader – ESP32 Batching System

## What it does
Controls **2 flowmeters** and **2 RS-485 shut-off valves** to load precise amounts of liquid product.  
An **ESP32** runs a WiFi access point with a built-in web server — open the page on any **Android phone** (or any browser) to operate the system.

## Pages

| Page | Purpose |
|------|---------|
| **Main** | Live view — shows product, target amount, progress bar, flowmeter readings, STOP button |
| **Load** | Select a product, enter litres, press START |
| **Setup** | Create / edit / delete products. Set flowmeter calibration (pulses/litre) and valve close-time (ms) per product |

## Hardware Wiring

| Component | ESP32 Pin |
|-----------|-----------|
| Flowmeter 1 pulse | GPIO 34 |
| Flowmeter 2 pulse | GPIO 35 |
| RS-485 TX | GPIO 17 |
| RS-485 RX | GPIO 16 |
| RS-485 DE (direction) | GPIO 4 |

Valve 1 = Modbus address 1, Valve 2 = address 2.  
Change any pin or address in `src/config.h`.

## WiFi

| Setting | Default |
|---------|---------|
| SSID | `BatchLoader` |
| Password | `batch1234` |
| IP | `192.168.4.1` |

Connect your phone to the WiFi, open **http://192.168.4.1** in Chrome.

## Build & Upload (PlatformIO)

```bash
# Build firmware
pio run

# Upload firmware to ESP32
pio run -t upload

# Upload web files (LittleFS) to ESP32
pio run -t uploadfs

# Serial monitor
pio device monitor
```

## Project Structure

```
├── platformio.ini          # PlatformIO config
├── src/
│   ├── main.cpp            # Entry point
│   ├── config.h            # Pin definitions & defaults
│   ├── database.h          # Product DB on LittleFS (JSON)
│   ├── flowmeter.h         # Interrupt-driven pulse counters
│   ├── valve.h             # RS-485 valve open/close
│   ├── batching.h          # Batch loading engine
│   └── webserver.h         # REST API + static file server
├── data/
│   └── www/
│       ├── index.html       # Single-page app
│       ├── style.css        # Mobile-first dark theme
│       └── app.js           # UI logic & API calls
└── README.md
```

## How Batching Works

1. User selects a product and enters litres on the **Load** page
2. ESP32 opens both valves and starts counting pulses
3. When flowmeter reading approaches target (minus the product's close-time offset), the corresponding valve is closed early to account for flow still in the pipe
4. Both valves closed → batch complete
5. **Main** page shows live progress via a polling API (every 500ms)

## Product Database

Products are stored in `/products.json` on LittleFS (survives power cycles). Each product stores:
- **Name** — display name
- **Calibration 1 & 2** — pulses per litre for each flowmeter
- **Close Time 1 & 2** — how many milliseconds before target to shut each valve
