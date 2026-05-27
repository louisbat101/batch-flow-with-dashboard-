# Batch Flow System - READY ✅

## Hardware Status
- **Master Board (241201)**: ✅ Running and broadcasting WiFi
- **Slave Board (241301)**: ✅ Running and ready for relay control
- **Communication**: ✅ WiFi HTTP JSON API working

## WiFi Networks

### Master AP (Control Interface)
- **SSID**: `BatchFlow-Master`
- **Password**: `batchflow123`
- **Purpose**: UI dashboard, batch control
- **Access**: http://192.168.4.1 (after connecting to this network)

### Slave AP (Relay Control)
- **SSID**: `FlowNode-Setup`
- **Password**: `flownode123`
- **Purpose**: Relay endpoints for master
- **Master connects to this for relay control**

## How to Use

### On Your Phone or Tablet:
1. Go to WiFi settings
2. Connect to **"BatchFlow-Master"** (password: `batchflow123`)
3. Open browser and go to: **http://192.168.4.1**
4. You should see the Batch Flow dashboard

### Dashboard Features:
- **Status Display**: Shows online status, current flow volume, batch state
- **Valve Selection**: Choose which valve (1-4) to use
- **Litres Input**: Set target batch size
- **Start Button**: Begin batching
- **Stop Button**: Stop batching

## Architecture

```
┌─────────────────────────────────────────────┐
│  Phone/Tablet Connected to BatchFlow-Master │
│         (WiFi SSID: BatchFlow-Master)       │
└──────────────────┬──────────────────────────┘
                   │
                   ▼
        ┌──────────────────────┐
        │   Master Board 241201│
        │  - Hosts UI          │
        │  - Controls Batching │
        │  - AP: BatchFlow-*   │
        └──────────┬───────────┘
                   │ (WiFi STA Mode)
                   ▼
        ┌──────────────────────┐
        │   Slave Board 241301 │
        │  - Relay Control     │
        │  - Flow Monitoring   │
        │  - AP: FlowNode-*    │
        └──────────────────────┘
```

## API Endpoints (On Master at 192.168.4.1)

- `GET /api/status` - Get current status (online, flow, batching state)
- `POST /api/start` - Start batch (params: valve, litres)
- `POST /api/stop` - Stop batch

## Serial Debug
To see what's happening:
- Master: `/dev/cu.usbmodem241201` @ 115200 baud
- Slave: `/dev/cu.usbmodem241301` @ 115200 baud

## Troubleshooting

### UI Not Loading
- Make sure you're connected to **"BatchFlow-Master"** WiFi
- Try refreshing the page
- Check that IP is 192.168.4.1 (not 192.168.5.1)

### Relays Not Responding
- Check master serial output for "[Poll]" messages
- Ensure slave is connected and visible

### Can't Find WiFi Network
- Check master board is plugged in (LED should be on)
- Wait 10 seconds for AP to fully initialize
- Try WiFi scan again

## Next Steps
- Test batch control through UI
- Verify relay activation
- Monitor flow readings if flowmeter connected
- Configure persistence for batch history
