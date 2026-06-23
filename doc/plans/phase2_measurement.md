# Phase 2 — Measurement & Calibration

## Goal

Configure the INA226 for accurate voltage and current readings.
Implement a driver that returns calibrated values.

## INA226 Configuration

| Parameter | Value | Register |
|---|---|---|
| Shunt resistor | 50 mΩ | Calibration (0x05) |
| Averaging | 64 samples | Config (0x00) |
| Bus voltage conversion | 1.1 ms | Config (0x00) |
| Shunt voltage conversion | 1.1 ms | Config (0x00) |
| Mode | Continuous shunt + bus | Config (0x00) |

With 64x averaging and 1.1 ms conversion for both channels:
Total conversion time ≈ 64 × (1.1 + 1.1) ms ≈ 141 ms per reading.
Well within the 10-second measurement interval.

## INA226 Resolution

- Bus voltage LSB: 1.25 mV → range 0–40.96V, resolution 1.25 mV
- Shunt voltage LSB: 2.5 µV → at 50 mΩ shunt, current LSB = 50 µA
- Max measurable current: 50 µA × 32767 = ±1.638A (sufficient for 0–1A range)

## Calibration Register

CAL = 0.00512 / (current_LSB × R_shunt)
CAL = 0.00512 / (0.00005 × 0.05) = 2048

## Software Tasks

1. **INA226 driver** (`ina226.h` / `ina226.cpp`)
   - `init()` — configure registers, write calibration
   - `readBusVoltage_mV()` — returns cell voltage in millivolts
   - `readCurrent_mA()` — returns current in milliamps
   - `readPower_mW()` — returns power in milliwatts (optional, INA226 calculates internally)
   - `isConversionReady()` — check alert/conversion ready flag

2. **Serial output for verification**
   - Print voltage, current, power every second during development
   - Compare against multimeter readings

## Calibration Procedure

1. No load (FETs off): read voltage, compare to multimeter on cell terminals
2. Known load (external precision resistor): read current, compare to multimeter in series
3. Adjust shunt value in software if systematic offset exists
4. Document calibration offset in `config.h`

## Acceptance Criteria

- [ ] Voltage reading within ±5 mV of multimeter
- [ ] Current reading within ±5 mA of multimeter at 1A
- [ ] Stable readings (low noise) with 64x averaging
- [ ] Zero current reading when FETs are off (< 2 mA)
