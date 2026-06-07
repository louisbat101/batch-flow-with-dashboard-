# ESP32-C3 Master - WiFi AP Setup Guide

## ✅ What's Installed
- **Clean WiFi AP firmware** (NO RS-485 code)
- **WebServer** on port 80
- **Web UI files** uploaded to LittleFS
- **Configured to broadcast:**
  - SSID: `BatchFlow-Master`
  - Password: `batchflow123`
  - IP Address: `192.168.4.1`

## 📱 How to Connect with Tablet

### Step 1: Power on the C3
- USB cable connected to `/dev/cu.usbmodem241301`
- Device boots and starts WiFi AP

### Step 2: Find the Network on Your Tablet
1. Open **Settings** → **WiFi**
2. Look for **"BatchFlow-Master"** in the network list
3. Tap to connect
4. Enter password: **`batchflow123`**

### Step 3: Open the Web Interface
1. Open **Safari** or **Chrome** on tablet
2. Go to: **`http://192.168.4.1`**
3. You should see the **Batch Flow Dashboard**

## 🔧 Firmware Location
- Source: `/Users/louishome/working projects/batch flow/esp32-c3-master/src/main.cpp`
- Uploaded to: `/dev/cu.usbmodem241301`

## 📊 Serial Debug Output
To see boot messages:
```bash
cat /dev/cu.usbmodem241301
```

Should show:
```
[3/4] Starting WiFi AP...
      Attempting to start AP...
      Attempt 1: ✅
      AP Final Status: ✅ BROADCASTING
      SSID: BatchFlow-Master (should appear on tablet WiFi list)
      Password: batchflow123
      IP Address: 192.168.4.1
```

## ⚠️ If WiFi AP Doesn't Appear

### Option 1: Check if it's a hidden network
- Try manually entering SSID: `BatchFlow-Master`
- Password: `batchflow123`

### Option 2: Check channel interference
- The AP broadcasts on **Channel 1 (2.4GHz)**
- Try moving closer to the ESP32
- Remove interference from other WiFi networks

### Option 3: Verify with Serial Monitor
```bash
cd /Users/louishome/working projects/batch flow/esp32-c3-master
pio device monitor -p /dev/cu.usbmodem241301 -b 115200
```

Should show "✅ BROADCASTING" at startup

## 🔄 Rebuild & Upload
```bash
cd /Users/louishome/working projects/batch flow/esp32-c3-master

# Build firmware
pio run -e esp32c3-master

# Upload firmware
pio run -e esp32c3-master -t upload --upload-port /dev/cu.usbmodem241301

# Upload web files
pio run -e esp32c3-master -t uploadfs --upload-port /dev/cu.usbmodem241301
```

## 📌 Key Files
- **Firmware:** `esp32-c3-master/src/main.cpp`
- **Config:** `esp32-c3-master/platformio.ini`
- **Web Files:** 
  - `esp32-c3-master/data/index.html`
  - `esp32-c3-master/data/style.css`
  - `esp32-c3-master/data/app.js`
  - `esp32-c3-master/data/dashboard.js`

## ✅ Status
- ✅ RS-485 code removed
- ✅ WiFi AP configured
- ✅ Web server ready
- ✅ Files uploaded to LittleFS
- ⏳ **Awaiting tablet test**
