# Phase 7 — Integration & Test Automation

## Goal

Combine all modules into a complete test flow, add user control via Serial
commands, and validate the full system end-to-end.

## Main Loop Architecture

```
setup()
  ├── init Serial (115200 baud)
  ├── init I2C + INA226
  ├── init FET GPIOs (LOW)
  ├── init WiFi
  └── print "READY"

loop()
  ├── check Serial for commands
  ├── state machine tick
  │   ├── IDLE: do nothing, wait for command
  │   ├── DISCHARGING: measure, integrate, log, check cutoff
  │   ├── DCIR: run DCIR sequence
  │   ├── DONE: report final results
  │   └── ERROR: report error, wait for reset
  └── handle WiFi reconnect if needed
```

## Serial Command Interface

| Command | Action |
|---|---|
| `start low` | Start discharge at ~0.5A |
| `start high` | Start discharge at ~1A |
| `stop` | Stop discharge, report results |
| `dcir` | Run single DCIR measurement |
| `status` | Print current voltage, current, state |
| `reset` | Return to IDLE state |

## Full Discharge Test Flow

1. User sends `start high`
2. ESP32 enters DISCHARGING state
3. Every 10 seconds:
   - Read voltage + current
   - Integrate capacity
   - Send HTTP GET with data
   - Print to Serial
4. Optionally: every 500 mAh, pause discharge, run DCIR, resume
5. When V < V_CUTOFF: stop discharge, enter DONE
6. Report: total capacity, energy, discharge time, final DCIR

## Test Scenarios

### Test 1: Basic measurement
- Connect charged cell, send `status`
- Verify voltage reads ~4.1V, current reads ~0 mA

### Test 2: Discharge low
- Send `start low`, let run for 60 seconds
- Verify current ~0.5A, voltage dropping
- Send `stop`, verify capacity > 0

### Test 3: Discharge high to cutoff
- Send `start high`, let run until cutoff
- Verify final capacity matches cell rating (±10%)

### Test 4: DCIR standalone
- Send `dcir`, verify R_i result is plausible

### Test 5: WiFi logging
- Run test 2 while monitoring server
- Verify all measurements arrive

### Test 6: WiFi dropout recovery
- Start discharge, disconnect WiFi AP, reconnect
- Verify measurements resume after reconnect
- Verify Serial logging continued during dropout

## Acceptance Criteria

- [ ] Full discharge test runs unattended to completion
- [ ] All Serial commands work as specified
- [ ] HTTP logging captures complete discharge curve
- [ ] Capacity result within 10% of cell datasheet rating
- [ ] No crashes or watchdog resets during multi-hour tests
- [ ] DCIR results logged at multiple SOC points (if enabled)
