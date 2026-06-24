# Phase 8 — PCB Design (Optional, Future)

## Goal

Design a dedicated PCB in KiCad once the breadboard prototype is validated.

## Scope (to be refined later)

- Schematic capture from validated breadboard circuit
- Component footprint selection
- PCB layout with attention to:
  - Two ground domains (GND_PWR, GND_MEAS) joined only at the cell − star point
  - Isolated DC-DC placement (barrier between power-bank side and logic side)
  - Kelvin (four-wire) sense traces for INA226 (VBUS→Cell+, GND→Cell−)
  - Thick traces / copper pours for discharge current paths (GND_PWR)
  - Thermal relief for power resistors
  - FET gate routing away from sense lines; gate pull-down to GND_MEAS
- Connector choices: spring-loaded cell holder or screw terminals
- Optional: NTC footprint for temperature sensing

## Not Started

This phase is deferred until phases 1–6 are complete and the circuit is validated.
Details will be filled in when the decision to proceed is made.
