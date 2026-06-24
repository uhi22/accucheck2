# accucheck2 — Development Plan Overview

## Project Summary

A LiIon battery cell tester that measures capacity (mAh), energy (Wh), and DC internal
resistance (DCIR) using an ESP32 + INA226, with WiFi-based logging via HTTP GET requests
to an external server.

## Key Design Decisions

| Decision | Choice |
|---|---|
| MCU | ESP32 |
| Framework | Arduino (C++) |
| Current sensor | INA226, 16-bit, I2C |
| Shunt resistor | 100 mΩ |
| Discharge levels | 1 (13.5Ω = 2×27Ω parallel, ~274 mA) |
| Logging | HTTP GET to external server, parameters in URL |
| Measurement interval | 10 seconds |
| Power supply | USB power bank PCB (cell → 5V step-up) |
| PCB | Breadboard/perfboard initially, KiCad later |

## Development Phases

| Phase | Description | Status | Details |
|---|---|---|---|
| 1 | Hardware setup & basic I2C | done | [phase1_hardware_bringup.md](phase1_hardware_bringup.md) |
| 2 | Measurement & calibration | done | [phase2_measurement.md](phase2_measurement.md) |
| 3 | Discharge control & capacity test | partial | [phase3_discharge_control.md](phase3_discharge_control.md) |
| 4 | DCIR measurement | done | [phase4_dcir.md](phase4_dcir.md) |
| 4b | DCIR measurement tooling | done | [phase4b_dcir_tooling.md](phase4b_dcir_tooling.md) |
| 5 | WiFi & HTTP logging | open | [phase5_wifi_logging.md](phase5_wifi_logging.md) |
| 6 | PHP server & visualization | open | [phase6_server.md](phase6_server.md) |
| 7 | Integration & test automation | open | [phase7_integration.md](phase7_integration.md) |
| 8 | (Optional) PCB design | open | [phase8_pcb.md](phase8_pcb.md) |

## Hardware Block Diagram

The 100 mΩ shunt sits directly at the cell's positive terminal, so it carries
**all** current the cell delivers — both the discharge resistor and the power
bank / ESP32 supply. VBUS senses the cell's positive terminal directly (Kelvin),
upstream of the shunt, so the measured voltage is the true cell terminal voltage.

```
  ┌─────────────┐
  │  LiIon Cell  │
  │  under test  │
  └──┬───────┬──┘
     │ (Cell+)│ (Cell-)
     │        │
     ├──── VBUS sense ──────────────────► INA226 VBUS (true cell voltage)
     │        │
  (IN+)       │
     │        │
 [Shunt 100mΩ]│         ◄── INA226 IN+/IN- measure TOTAL cell current
     │        │
  (IN-)       │
     │        │
     ●━━ node ━━━━━━━━━━━━━━━━━━━━━━━━┓   │
     │        │                       │   │
     ├──┌─────┴──────────┐            │   │
     │  │  USB Power Bank  │           │   │
     │  │  PCB (step-up)   │── 5V ── ESP32 VIN
     │  └─────────────────┘           │   │
     │                          ┌─────┴───┴┐
     ├── 13.5Ω (2×27Ω) ── FET ─│  ESP32    │── WiFi ── HTTP GET ── Server
     │              (~274mA)    │  (I2C +   │
     │                          │   GPIO)   │
     │                          └─────┬─────┘
     │                                │
     └──────────────── GND ───────────┴──── (Cell-)
```

Note: the discharge-resistor FET source and the power-bank/ESP32 ground all
return to the common GND (Cell-). The shunt is the single point through which
the entire cell current flows, so capacity (mAh) and energy (Wh) include the
ESP32's own consumption.

## File Structure (planned)

```
accucheck2/
├── readme.md
├── doc/
│   ├── plans/
│   │   ├── overview.md              (this file)
│   │   ├── phase1_hardware_bringup.md
│   │   ├── phase2_measurement.md
│   │   ├── phase3_discharge_control.md
│   │   ├── phase4_dcir.md
│   │   ├── phase4b_dcir_tooling.md
│   │   ├── phase5_wifi_logging.md
│   │   ├── phase6_server.md
│   │   ├── phase7_integration.md
│   │   └── phase8_pcb.md
│   └── reports/
│       └── dcir_report_*.html       (measurement reports)
├── tools/
│   └── dcir_plot.py                 (DCIR measurement & plotting tool)
├── webspace/
│   ├── add.php                      (data receiver endpoint)
│   ├── data.php                     (data API for visualization)
│   ├── index.php                    (visualization page)
│   └── data/.htaccess               (deny direct browsing)
└── arduinosketch/
    └── accucheck2/
        ├── accucheck2.ino           (main sketch)
        ├── ina226.h / ina226.cpp    (INA226 driver)
        ├── discharge.h / .cpp       (FET control, discharge logic)
        ├── dcir.h / dcir.cpp        (DCIR measurement routine)
        ├── logger.h / logger.cpp    (WiFi + HTTP GET logging)
        ├── config.h                 (pins, calibration, WiFi credentials)
        └── config.h.example         (template for config.h)
```
