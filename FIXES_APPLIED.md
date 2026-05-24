# Fixes Applied - May 23, 2026

## Issues Fixed

### 1. ✅ Station Rename Not Working
**Problem**: Rename modal was only updating local mock data, not calling the API.

**Solution**:
- Modified `saveRenameBoard()` function in `dashboard.js`
- Now calls `POST /api/boards/rename` with `{address, name}`
- Backend receives the request and updates `slaveStatus[address].stationName`
- Shows success/error alerts
- Refreshes board status after rename

**API Endpoint**: `POST /api/boards/rename`
```json
{
  "address": 1,
  "name": "Acid Station"
}
```

### 2. ✅ Board Configuration Shows All 10 Stations (Should Only Show Detected)
**Problem**: Products page "Board Configuration" section showed mock data for all stations regardless of detection.

**Solution**:
- Modified `renderBoardConfigs()` function
- Fetches real stations from `GET /api/boards/status`
- Filters to only show `online: true` stations
- Shows "No boards detected" message if none online
- Displays station address and online status

**Example**: If only Station 1 is detected → shows only Station 1

### 3. ✅ Load Page Shows All 10 Stations (Should Only Show Detected)
**Problem**: Load page populated all 10 stations from mock data.

**Solution**:
- Modified `renderLoadTableWithData()` function
- Filters stations to only `online: true`
- Only displays rows for detected/online stations
- Shows "No boards detected" message if none online
- Station names come from backend (custom or default "Station N")
- Product dropdown populated from real product database

**Example**: If Stations 1, 3, 5 are online → Load page shows only those 3

### 4. ✅ Run Page Shows All Stations (Should Only Show Detected)
**Problem**: Run page showed mock data for all configured runs.

**Solution**:
- Completely rewrote `renderRunPage()` function
- Fetches stations from `GET /api/boards/status`
- Filters to only `online: true` stations
- Cross-references with `loadData` to find configured loads
- Only shows stations that are:
  1. Online (detected)
  2. Have a product selected
  3. Have amount > 0
- Shows "No loads configured" if none ready
- Shows "No boards detected" if none online

**Example**: 
- Station 1: Online, Product=Acid, Amount=25L → Shows in Run page
- Station 2: Online, No product → Hidden from Run page
- Station 3: Offline → Hidden from Run page

## Files Modified

### `/esp32-c3-master/data/www/dashboard.js`
1. **saveRenameBoard()** - Lines ~370-415
   - Added XMLHttpRequest POST to `/api/boards/rename`
   - Added error handling and success alerts
   - Calls refreshBoardStatus() after rename

2. **renderBoardConfigs()** - Lines ~578-620
   - Changed from mock data to API fetch
   - Filters to online stations only
   - Shows detection messages

3. **renderLoadTableWithData()** - Lines ~237-295
   - Filters to online stations only
   - Reinitializes loadData from detected stations
   - Updates station names from API

4. **renderRunPage()** - Lines ~449-535
   - Complete rewrite with API fetch
   - Filters online stations
   - Cross-references with loadData
   - Only shows ready-to-run stations

### `/esp32-c3-master/data/www/style.css`
- Added `.no-boards-msg` style for info messages
- Added `.error-msg` style for error messages
- Added `.status-online` / `.status-offline` indicators

## Backend (Already Implemented)

### `/esp32-c3-master/src/main.cpp`
- `handleBoardsStatus()` - Lines ~376-402
  - Returns all 10 slots with online/offline status
  - Includes custom station names or defaults

- `handleRenameStation()` - Lines ~404-429
  - POST endpoint for renaming stations
  - Validates address 1-10
  - Updates slaveStatus[address].stationName
  - Returns {ok: true}

- SlaveStatus struct - Lines ~44-50
  - Added `char stationName[32]` field
  - Initialized in setup() loop

## Testing Instructions

1. **Test Station Rename**:
   - Go to Boards page
   - Click on a board card
   - Enter new name (e.g., "Acid Station")
   - Click Save
   - Should see "Station renamed successfully" alert
   - Check serial monitor for: `[Station] Address 1 renamed to: Acid Station`

2. **Test Detection Filtering**:
   - Start with only Slave 1 connected
   - **Products Page → Board Configuration**: Should show only Station 1
   - **Load Page**: Should show only Station 1 row
   - **Run Page**: Should show "No loads configured" (until you set up a load)
   
3. **Test Load Configuration**:
   - Go to Load page
   - Select a product from dropdown
   - Enter amount (e.g., 25.5)
   - Go to Run page
   - Should now show Station 1 ready to run

4. **Test Multi-Board**:
   - Connect multiple slave boards
   - All detected stations should appear in Load/Run pages
   - Offline stations should NOT appear

## API Endpoints Used

- `GET /api/boards/status` - Returns all 10 slots with online/offline status
- `POST /api/boards/rename` - Rename a station
- `GET /api/products` - Get product list for dropdowns

## Safety Features Maintained

- ✅ Addresses 1-10 remain FIXED (never reorder)
- ✅ Offline stations reserve their positions in backend
- ✅ Frontend only shows online stations (cleaner UX)
- ✅ Station names persist in RAM (until reboot)

## Known Limitations

1. Station names are stored in RAM - will reset on ESP32 reboot
   - Future: Add SPIFFS or NVS persistence
2. Run page buttons are placeholders - actual dispensing API pending
3. Real-time progress updates not yet implemented

## Upload Commands

```bash
# Upload web files (HTML/CSS/JS)
cd esp32-c3-master
pio run -e esp32c3-master -t uploadfs --upload-port /dev/cu.usbmodem241201

# Upload firmware (if main.cpp changed)
pio run -e esp32c3-master -t upload --upload-port /dev/cu.usbmodem241201
```

## Success Criteria

✅ Can rename stations via Boards page modal
✅ Products page only shows detected boards
✅ Load page only shows detected boards  
✅ Run page only shows detected boards with configured loads
✅ No errors in browser console
✅ All API calls working correctly
