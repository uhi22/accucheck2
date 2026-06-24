# Phase 3 — Discharge Control & Capacity Test

## Goal

Implement controlled discharge with FET switching, integrate measured current
over time to calculate cell capacity (mAh) and energy (Wh).

## Discharge Logic

### State Machine

```
  IDLE ──(start)──► DISCHARGING ──(V < V_cutoff)──► DONE
                        │                              │
                        │──(error/timeout)──► ERROR     │
                        │                              │
                        └──(manual stop)──► IDLE ◄─────┘
```

### Parameters (in `config.h`)

| Parameter | Default | Unit |
|---|---|---|
| V_CUTOFF | 3000 | mV (safe lower limit for LiIon) |
| DISCHARGE_RESISTOR | 13.5Ω (2×27Ω parallel) | single discharge path (~274 mA at 3.7V) |
| MEASURE_INTERVAL | 10000 | ms |
| MAX_DISCHARGE_TIME | 259200 | seconds (3 days safety limit) |

## Capacity Calculation

Trapezoidal integration of current over time:

```
capacity_mAh += (I_prev + I_now) / 2.0 * dt_hours
```

where `dt_hours = (t_now - t_prev) / 3600000.0`

## Software Tasks

1. **Discharge controller** (`discharge.h` / `discharge.cpp`)
   - `dischargeInit()` — set FET off, state to IDLE
   - `dischargeStart()` — begin capacity test
   - `dischargeStop()` — turn off FET, report result
   - `dischargeTick()` — called from loop(), reads voltage/current every MEASURE_INTERVAL, integrates capacity and energy, checks cutoff

2. **Safety checks in `tick()`**
   - Stop if voltage < V_CUTOFF
   - Stop if current is unexpectedly high (> 2A)
   - Stop if MAX_DISCHARGE_TIME exceeded
   - Stop if voltage reads 0 (cell disconnected)

3. **Serial reporting**
   - Every measurement: timestamp, voltage, current, cumulative capacity, cumulative energy
   - On completion: total capacity (mAh), total energy (Wh), average current, discharge time

## Energy Calculation

Trapezoidal integration of power over time:

```
energy_mWh += (P_prev + P_now) / 2.0 * dt_hours
```

where `P = V * I` (in mW), same `dt_hours` as above.

## Acceptance Criteria

- [x] FET switches on/off cleanly
- [ ] Discharge stops at V_CUTOFF (not yet tested to completion)
- [ ] Capacity (mAh) matches expectation for known cell (within ~5%)
- [ ] Energy (Wh) matches expectation for known cell (within ~5%)
- [ ] Safety timeout works
- [x] Serial commands (start/stop/status/reset) working
- [x] Capacity and energy integration running correctly
