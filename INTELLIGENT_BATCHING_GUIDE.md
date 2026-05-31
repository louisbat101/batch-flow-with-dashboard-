# Intelligent Batching System - Technical Implementation

## Overview

The BatchFlow intelligent batching system uses **pulse-count based control** with **adaptive shutoff** to achieve accurate dispensing across multiple products without manual valve timing adjustments.

---

## Calibration

### Flow Meter Calibration Formula

```
Actual PPG (Pulses Per Gallon) = Total Pulses / Measured Volume

Example:
- Dispense product into 1-gallon container
- Count flowmeter pulses: 450 pulses
- Calibrated PPG = 450 / 1.0 = 450 PPG
```

### Convert Between Units

```
PPG (Pulses Per Gallon) → PPL (Pulses Per Liter)
PPL = PPG / 3.785

Example:
PPL = 450 / 3.785 = 118.9 PPL
```

---

## Batching Logic

### Target Pulse Calculation

```
Target Pulses = Target Gallons × Pulses Per Gallon

Example:
Target: 5 gallons, PPG: 450
Target Pulses = 5 × 450 = 2250 pulses
```

### Shutoff Point Calculation

Two modes available:

#### Mode 1: Manual Shutoff (Fixed Offset)

Used when operator wants predictable, simple control.

```
Shutoff Point (pulses) = Target Pulses - (Offset Gallons × PPG)

Example:
Offset: 0.5 gallons
Shutoff Point = 2250 - (0.5 × 450) = 2025 pulses
Valve closes after 2025 pulses
```

#### Mode 2: Intelligent Shutoff (Dynamic Offset)

Automatically calculates shutoff based on **measured flow rate** and **valve close time**.

```
Dynamic Offset (gallons) = Flow Rate (GPS) × Valve Close Time (seconds)

Where:
GPS = Gallons Per Second = (Latest Pulses / Calibrated PPG) / Time Delta

Full Formula:
Dynamic Offset = (Flow Rate GPM / 60) × Valve Close Time

Shutoff Point = Target Pulses - (Dynamic Offset × PPG)

Example:
Measured Flow: 2.5 GPM
Valve Close Time: 0.5 seconds
Dynamic Offset = (2.5 / 60) × 0.5 = 0.0208 gallons
Offset Pulses = 0.0208 × 450 = 9.4 pulses
Shutoff Point = 2250 - 9.4 = 2240.6 pulses
```

---

## Adaptive Learning

After each batch, the system measures the error and adjusts future batches.

### Error Calculation

```
Error = Actual Volume - Target Volume

Example:
Target: 5.0 gallons
Actual: 5.08 gallons (overshot by 0.08 gal)
Error = 5.08 - 5.0 = +0.08 gallons
```

### Learning Correction Update

```
Learn Correction += Error × Learning Gain

Typical Learning Gain: 0.1 (10% adjustment per batch)

Example (Batch 1):
Error = +0.08 gallons
Learn Correction = 0 + (0.08 × 0.1) = +0.008 gallons

Example (Batch 2):
Error = -0.05 gallons
Learn Correction = 0.008 + (-0.05 × 0.1) = 0.003 gallons
```

### Correction Limits

```
-2.0 gallons ≤ Learn Correction ≤ +2.0 gallons
```

---

## Complete Shutoff Formula

### Recommended Implementation

```
Close Point (gallons) = 
    Target Volume 
    - Manual Offset (if in manual mode)
    - (Flow Rate GPS × Valve Close Time)
    - Learn Correction

Close Point (pulses) = Close Point (gallons) × PPG
```

### Example Scenario

**Setup:**
- Target: 5.0 gallons
- PPG: 450
- Valve Close Time: 0.5 seconds
- Manual Offset: 0.5 gallons (fallback for intelligent mode)
- Learning Enabled: Yes
- Learning Gain: 0.1
- Current Flow: 2.5 GPM
- Prior Learn Correction: 0.005 gallons

**Calculation:**

1. **Calculate Dynamic Offset:**
   - Flow Rate GPS = 2.5 / 60 = 0.0417 GPS
   - Dynamic Offset = 0.0417 × 0.5 = 0.0208 gallons

2. **Add Learning:**
   - Total Offset = 0.0208 + 0.005 = 0.0258 gallons

3. **Calculate Close Point:**
   - Close Point (gallons) = 5.0 - 0.0258 = 4.974 gallons
   - Close Point (pulses) = 4.974 × 450 = 2238.3 pulses

4. **Shut Off When:**
   - Pulses Received ≥ 2238 pulses
   - Valve closes immediately
   - System stops counting after reaching target

---

## Live Flow Rate Monitoring

### Real-time Flow Rate Calculation

```
Update Interval: Every 100-200ms

For each interval:
  Delta Time (seconds) = (Current Time - Last Time) / 1000
  Delta Pulses = New Pulses - Previous Pulses
  Delta Gallons = Delta Pulses / PPG
  
  Flow Rate (GPS) = Delta Gallons / Delta Time
  Flow Rate (GPM) = Delta Gallons / Delta Time × 60
  Flow Rate (LPM) = Delta Gallons × 3.785 / Delta Time × 60
```

---

## Product Support

### Thin Products (Water, Alcohol)
- High flow rate (5-10 GPM)
- Smaller dynamic offset
- Faster control response
- Example: 450 PPG, 0.5s valve time = 9.4 pulse offset

### Thick Products (Oil, Syrup)
- Lower flow rate (0.5-2 GPM)
- Larger dynamic offset due to time
- Slower control response
- Example: Same 450 PPG, 0.5s valve time = 9.4 pulse offset (same!)
- **BUT:** At 0.5 GPM, 0.5s = 0.004 gallons vs at 5 GPM, 0.5s = 0.042 gallons
- System automatically compensates!

---

## HMI Display Information

### Essential Metrics (Always Show)

1. **Current Volume** (gallons)
   - Display: X.XXX gal
   - Updates: Every 100-200ms

2. **Target Volume** (gallons)
   - Display: 5.000 gal
   - Set by operator

3. **Progress** (percent)
   - Display: 45%
   - Formula: (Current / Target) × 100

4. **Flow Rate** (GPM)
   - Display: 2.5 GPM
   - Updates: Every 100-200ms
   - Shows real-time flow

### Advanced Metrics (Intelligent Mode)

5. **Shutoff Point** (gallons)
   - Display: 4.975 gal
   - Calculated: Target - Total Offset

6. **Dynamic Offset** (gallons)
   - Display: 0.021 gal
   - Calculated: Flow × Valve Close Time

7. **Learn Correction** (gallons)
   - Display: +0.004 gal
   - Accumulated from previous batches

### Batch Results (After Completion)

8. **Actual Volume** (gallons)
   - Display: 5.008 gal
   - Final measured volume

9. **Batch Error** (gallons and percent)
   - Display: +0.008 gal (0.16%)
   - Formula: Actual - Target

10. **Status**
    - PASS: ±0.05 gallons tolerance
    - FAIL: Outside tolerance

---

## Implementation Checklist

- [x] PPG calibration function
- [x] Target pulse calculator
- [x] Flow rate monitor (real-time)
- [x] Manual shutoff mode
- [x] Dynamic shutoff mode (flow × valve time)
- [x] Adaptive learning correction
- [x] Batch error tracking
- [x] Learn correction limits (-2 to +2 gallons)
- [x] HMI display for all metrics
- [x] History logging
- [ ] Integration with RS-485 valve control
- [ ] Integration with flowmeter pulse ISR
- [ ] Persistent storage of calibration data
- [ ] WebSocket updates for real-time display

---

## Performance Targets

- **Accuracy:** ±0.05 gallons (±1%)
- **Repeatability:** <0.1 gallons variation across 10 batches
- **Settling Time:** <100ms from shutoff to idle
- **Response Time:** <50ms from shutoff command to valve movement
- **Products Supported:** 1-2000 PPG range (0.5-500 PPL)
- **Flow Range:** 0.1 - 20 GPM

---

## Reference: Valve Close Time Measurement

To calibrate valve close time:

1. **Prime the system** with water
2. **Send CLOSE command** and immediately **start timer**
3. **Stop timer** when flow stops completely
4. **Measured Time** = Valve Close Time

Example:
- Start: 0ms
- Stop: 520ms
- Valve Close Time = 0.52 seconds

This is typically 0.3-1.0 seconds depending on valve model.

---

## References

- **File:** `esp32-c3-master/src/batching_control.h`
- **UI File:** `esp32-c3-master/data/batching.html`
- **JavaScript:** `esp32-c3-master/data/batching.js`

