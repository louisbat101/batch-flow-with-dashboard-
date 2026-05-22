<!-- ESP32 Batching System -->

# Batch Loader – Project Instructions

## Stack
- **Platform**: ESP32 (PlatformIO + Arduino framework)
- **Storage**: LittleFS (persistent product database as JSON)
- **Comms**: RS-485 (valves), pulse interrupts (flowmeters)
- **Web**: ESPAsyncWebServer serving SPA from LittleFS `/www/`
- **UI**: Vanilla HTML/CSS/JS, mobile-first dark theme

## File Map
- `src/config.h` — all pin definitions, addresses, defaults
- `src/database.h` — Product struct, JSON CRUD on LittleFS
- `src/flowmeter.h` — ISR-based pulse counting
- `src/valve.h` — RS-485 open/close commands
- `src/batching.h` — batch state machine (IDLE → RUNNING → DONE)
- `src/webserver.h` — REST API endpoints
- `src/main.cpp` — setup + loop
- `data/www/` — web UI uploaded to LittleFS

## Build Commands
- `pio run` — compile
- `pio run -t upload` — flash firmware
- `pio run -t uploadfs` — upload web files to LittleFS
