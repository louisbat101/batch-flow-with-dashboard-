# BatchFlow Intelligent Batching - Quick Reference

## Key Formulas

### 1. Calibration (Setup)
```
PPG (Pulses Per Gallon) = Total Pulses / Measured Gallons
```

### 2. Target Calculation
```
Target Pulses = Target Gallons × PPG
```

### 3. Intelligent Shutoff Offset
```
Dynamic Offset (gallons) = (Flow Rate GPM / 60) × Valve Close Time (seconds)
```

### 4. Shutoff Point
```
Shutoff Pulses = Target Pulses - (Dynamic Offset × PPG)
```

### 5. Adaptive Learning
```
Learn Correction += Error × Learning Gain
Limit: -2.0 to +2.0 gallons
```

---

## Complete Shutoff Formula (Used in System)

```
ClosePoint = Target - (FlowRateGPS × ValveCloseTime) - LearnCorrection
ClosePointPulses = ClosePoint × PPG
```

---

## How It Works

### Before Batch
1. Operator selects product (450 PPG example)
2. Enters target volume (5 gallons)
3. System calculates: 5 × 450 = 2250 pulses target

### During Batch
1. Valve opens, pulses flow in
2. System monitors real-time flow rate every 100ms
3. When pulses ≥ shutoff point → valve closes

### After Batch
1. System measures actual volume dispensed
2. Calculates error: Actual - Target
3. Updates learn correction for next batch

---

## Two Modes

### Manual Mode (Fixed)
- Operator sets offset: 0.5 gallons
- Shutoff: Always at (Target - 0.5 gallons)
- Good for: Testing, stable conditions

### Intelligent Mode (Dynamic) ← **RECOMMENDED**
- Automatically calculates offset from flow
- Adapts to viscosity, pressure, temperature
- Automatically learns from each batch
- Good for: Production, variable products

---

## Configuration Parameters

| Parameter | Range | Default | Notes |
|-----------|-------|---------|-------|
| Pulses Per Gallon | 10-2000 | 450 | Product-specific |
| Valve Close Time | 0.1-2.0 sec | 0.5 | Measured per valve |
| Manual Offset | 0-5 gal | 0.5 | Fallback/testing |
| Learning Gain | 0.0-1.0 | 0.15 | 15% per batch |
| Target Volume | 0.1-1000 gal | 5.0 | Per batch |

---

## Display Information

**Essential (Always Show):**
- Current Volume: 4.234 gal
- Target Volume: 5.000 gal  
- Flow Rate: 2.5 GPM
- Progress: 85%

**Advanced (Intelligent Mode):**
- Shutoff Point: 4.974 gal
- Dynamic Offset: 0.021 gal
- Learn Correction: +0.005 gal

**Results (After Batch):**
- Actual: 5.008 gal
- Error: +0.008 gal (0.16%)
- Status: ✅ PASS

---

## Example Scenario

**Setup:**
- Product: Acid, 450 PPG
- Target: 5.0 gallons
- Mode: Intelligent
- Valve Close Time: 0.5 seconds
- Learning Enabled

**Batch #1:**
- Flow Rate: 2.5 GPM
- Dynamic Offset: (2.5/60) × 0.5 = 0.021 gal
- Shutoff Point: 5.0 - 0.021 = 4.979 gal = 2240 pulses
- Actual: 5.008 gal
- Error: +0.008 gal
- Learn Correction: 0 + (0.008 × 0.15) = +0.0012 gal

**Batch #2:**
- Same flow rate
- Dynamic Offset: 0.021 + 0.0012 = 0.0222 gal
- Shutoff Point: 5.0 - 0.0222 = 4.978 gal = 2239 pulses
- Actual: 5.003 gal (closer!)
- Error: +0.003 gal
- Learn Correction: 0.0012 + (0.003 × 0.15) = 0.0017 gal

**Result:** Each batch gets more accurate!

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| Over-dispensing | Shutoff too late | ↑ Increase offset or learning gain |
| Under-dispensing | Shutoff too early | ↓ Decrease offset or learning gain |
| Inconsistent | Valve quality | Check valve for leaks |
| Not learning | Learning disabled | Enable adaptive learning |
| Oscillating | Gain too high | Reduce learning gain (0.05-0.10) |

---

## Files Reference

| File | Purpose |
|------|---------|
| `src/batching_control.h` | C++ controller class |
| `data/batching.html` | Professional UI page |
| `data/batching.js` | UI logic + calculations |
| `INTELLIGENT_BATCHING_GUIDE.md` | Full documentation |

---

## Next Steps

1. **Integrate with Flowmeter ISR**
   - Call `receivePulse()` on each pulse
   - Update display with `getFlowRateGPM()`

2. **Connect Valve Control**
   - Call `shouldShutoff()` in main loop
   - Trigger valve close when true

3. **Persistent Storage**
   - Save PPG calibration to NVS
   - Save learn corrections between sessions

4. **WebSocket Updates**
   - Real-time metrics to browser
   - Batch history sync

---

## Contact & Support

For questions or issues, see:
- GitHub: https://github.com/louisbat101/batch-flow-with-dashboard-
- Documentation: INTELLIGENT_BATCHING_GUIDE.md
