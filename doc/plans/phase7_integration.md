# Phase 7 — Further Improvements

## Goal

Bring the device to a standalone, fully-featured state: auto-start on cell
connection, periodic DCIR during discharge, additional web page charts,
and DCIR detail visualization.

## Auto-Start

The device shall start the discharge measurement automatically when a cell is
connected — no serial command or button required. Detection logic:

1. On boot (or when in IDLE), periodically read the cell voltage
2. If voltage is in the valid LiIon range (e.g. 3000–4250 mV) and stable for
   a few seconds, auto-start the discharge
3. Run an initial DCIR measurement before starting the discharge
4. Serial commands (`stop`, `status`, `dcir`, `reset`) remain available for
   manual override

This makes the device usable as a standalone tool — just plug in a cell and
watch the results on the web page.

## Main Loop Architecture

```
setup()
  ├── init Serial (115200 baud)
  ├── init I2C + INA226
  ├── init FET GPIO (LOW)
  ├── init WiFi
  └── print "READY"

loop()
  ├── check Serial for commands
  ├── state machine tick
  │   ├── IDLE: monitor voltage, auto-start when cell detected
  │   ├── DISCHARGING: measure, integrate, log, check cutoff
  │   ├── DCIR: run DCIR sequence
  │   ├── DONE: report final results
  │   └── ERROR: report error, wait for reset
  └── handle WiFi reconnect if needed
```

## Serial Command Interface

| Command | Action |
|---|---|
| `start` | Start discharge manually |
| `stop` | Stop discharge, report results |
| `dcir` | Run single DCIR measurement |
| `status` | Print current voltage, current, state |
| `reset` | Return to IDLE state |

## Full Discharge Test Flow

1. Cell connected, auto-detected (or user sends `start`)
2. Initial DCIR measurement
3. ESP32 enters DISCHARGING state
4. Every 1 minute: pause discharge, run DCIR, resume discharge
   - FET off → DCIR measurement (~3s) → FET on
   - Capacity/energy integration continues correctly: during the DCIR pause
     the current drops to quiescent level, which is still measured and integrated
     (no gap in the trapezoidal integration, just lower current during the pause)
3. Every 10 seconds:
   - Read voltage + current
   - Integrate capacity
   - Send HTTP GET with data
   - Print to Serial
4. Optionally: every 500 mAh, pause discharge, run DCIR, resume
5. When V < V_CUTOFF: stop discharge, enter DONE
6. Report: total capacity, energy, discharge time, final DCIR

## Test Scenarios

### Test 1: Auto-start
- Connect charged cell, verify auto-detection and discharge start
- Verify initial DCIR measurement runs before discharge begins

### Test 2: Full discharge to cutoff
- Let auto-started discharge run until cutoff
- Verify final capacity matches cell rating (±10%)

### Test 3: Manual control
- Send `stop` during discharge, verify it stops
- Send `start`, verify it resumes with fresh counters
- Send `dcir`, verify standalone DCIR works

### Test 4: WiFi logging
- Run discharge while monitoring web page
- Verify all measurements arrive and charts update live

### Test 5: WiFi dropout recovery
- Start discharge, disconnect WiFi AP, reconnect
- Verify measurements resume after reconnect
- Verify Serial logging continued during dropout

## Acceptance Criteria

- [ ] Auto-start: discharge begins automatically when cell is connected
- [ ] Periodic DCIR every 1 minute during discharge
- [ ] Web page: energy vs. time chart
- [ ] Web page: R_i vs. time chart
- [ ] Web page: DCIR detail chart (fast-sampled voltage/current during load step)
- [ ] ESP32 sends DCIR samples batch via HTTP GET
- [ ] Connectivity watchdog: recover WiFi on the fly on persistent HTTP/WiFi
      failure, else full device reset (which stops the discharge and starts
      fresh on the next boot)
- [x] Hardware task watchdog (TWDT) resets the device if the firmware hangs
      (covers the case the software watchdog cannot — loop not running)
- [x] Onboard LED status indication (heartbeat / state feedback)
- [ ] Re-think the hardware schematic visualization (the Mermaid flowchart
      conveys the topology but reads oddly as an electrical schematic — evaluate
      a proper schematic export, e.g. KiCad, or a cleaner notation)
- [ ] Full discharge test runs unattended to completion
- [ ] Capacity result within 10% of cell datasheet rating
- [ ] No crashes or watchdog resets during multi-hour tests
