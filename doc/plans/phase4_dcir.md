# Phase 4 — DCIR Measurement (DC Internal Resistance)

## Goal

Measure the cell's internal resistance using the load-step method (DCIR),
leveraging the FET-switched discharge resistor as the load.
Log voltage and current over time during the DCIR sequence for visualization
in the web GUI.

## DCIR Method

**Note:** The cell also powers the ESP32 via the power bank PCB, and because the
power bank taps in downstream of the shunt, this quiescent current I_rest is
measured even with the FET off. The DCIR formula uses the *difference* in
voltage and current between rest and loaded states, so this baseline draw cancels
out.

**Important for accuracy:** the discharge load returns to the cell − terminal via
GND_PWR (not through the power bank), so the load-step current does **not** flow
through the power bank's internal protection FET. Only the ~constant ESP supply
current crosses that FET, and it cancels in the delta — so the protection FET's
Rds does not inflate R_i. See the grounding architecture in
[overview.md](overview.md#hardware-block-diagram).

```
1. FET off, cell supplies only quiescent load (power bank + ESP32)
2. Measure rest state:               V_rest, I_rest
3. Switch on load (FET):             wait for settling (~100 ms)
4. Measure loaded state:             V_load, I_load
5. Switch off load

R_internal = (V_rest - V_load) / (I_load - I_rest)
```

### Four-Wire Advantage

Because the INA226 senses voltage directly at the cell terminals (VBUS Kelvin to
Cell+, GND via GND_MEAS to Cell−) and the measurement ground carries essentially
no current, contact resistance of clips/wires and supply-current IR drops are
excluded from the R_internal measurement. This is only true with the two-domain
grounding (GND_PWR / GND_MEAS joined only at Cell−); see
[overview.md](overview.md#hardware-block-diagram).

## Timing

```
         FET off          FET on           FET off
           │                │                │
    ───────┤  ← 2s rest →  ├── 200ms settle ┤
           │     ↓          │     ↓          │
      V_rest, I_rest     V_load, I_load    done
```

- Rest period before measurement: 2 seconds (let cell voltage stabilize)
- Load settling time: 200 ms (RC effects in FET + wiring)
- Multiple measurements averaged for reliability

## INA226 Configuration for DCIR

During DCIR, use faster settings for better time resolution:
- Averaging: 16 samples (instead of 64)
- Conversion time: 332 µs (instead of 1.1 ms)
- Total per reading: ~11 ms — fast enough for 200 ms settling window

Restore normal config after DCIR measurement.

## Software Tasks

1. **DCIR module** (`dcir.h` / `dcir.cpp`)
   - `measureDCIR()` — full DCIR sequence, returns R_i in mΩ
   - Steps:
     1. Ensure FET off, wait 2s rest
     2. Sample V and I at ~11 ms intervals during rest (last ~220 ms), store 20 samples in RAM
     3. Read V_rest and I_rest (average of last 5 rest samples)
     4. Switch on the load FET
     5. Sample V and I at ~11 ms intervals for ~220 ms settling, store 20 samples in RAM
     6. Read V_load and I_load (average of last 5 loaded samples)
     7. Switch off FET
     8. Sample V and I for ~220 ms recovery, store 20 samples in RAM
     9. Calculate R_i = (V_rest - V_load) / (I_load - I_rest) × 1000 (in mΩ)
     10. Send all collected samples (~60 points) in a single HTTP GET to server
   - Batch format: comma-separated V:I pairs with relative timestamps, e.g.
     `GET /add.php?state=dcir&ri=42&samples=0:4185:12,11:4183:13,...`
   - This avoids disrupting the fast sampling loop with network I/O

2. **Plausibility checks**
   - R_i should be 10–500 mΩ for a typical LiIon cell
   - I_load should be > 100 mA (otherwise division is noisy)
   - V_drop should be > 5 mV (otherwise below measurement noise)

3. **Optional: DCIR at multiple SOC points**
   - During a discharge test, pause periodically to measure DCIR
   - e.g., every 10% SOC or every 200 mAh discharged
   - Log R_i vs. SOC curve

## Expected Results

| Cell condition | Typical R_i |
|---|---|
| New 18650 | 20–50 mΩ |
| Aged 18650 | 50–150 mΩ |
| Damaged cell | > 200 mΩ |

## Acceptance Criteria

- [x] DCIR measurement completes in < 5 seconds
- [x] R_i result is repeatable (spread 2.5 mOhm across 5 runs)
- [x] Result plausible for known cell (~96 mOhm)
- [ ] Four-wire measurement confirmed (adding wire resistance doesn't change R_i)
- [ ] DCIR voltage/current time series visible in web GUI as a chart (DCIR detail chart — phase 7)
- [x] Voltage step and current step clearly visible in the Plotly chart
