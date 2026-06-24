# Phase 1 — Hardware Setup & Basic I2C

## Goal

Wire up the ESP32 and INA226 on a breadboard, confirm I2C communication,
and read raw register values from the INA226.

## Grounding Architecture (two domains)

All grounds fan out from a single physical point — the cell's **negative
terminal** (the Kelvin star point). Two separate grounds start there and
**meet nowhere else**:

**GND_PWR — power ground (carries the heavy/supply currents):**
- Discharge FET source (load return, ~274 mA+ through the 13.5 Ω path)
- Power-bank battery-negative B− (the current the cell delivers to run the
  boost / ESP)
- Via the power-bank internals, the power-bank output P− and the isolated
  DC-DC *primary* ground sit in this domain too (bonded to Cell− through the
  bank's protection FET)

**GND_MEAS — measurement ground (carries essentially no current):**
- Isolated DC-DC *secondary* ground (the clean logic ground)
- ESP32 ground
- INA226 GND pin (also its bus-voltage reference = low side of the 4-wire
  cell-voltage sense)
- FET gate-driver / GPIO reference and the gate pull-down

The **isolated DC-DC** is the only barrier, and it sits in the power path:
power-bank OUT+/P− (primary) → isolated DC-DC → 5 V_iso / GND_MEAS (secondary).
Because the barrier blocks DC, the ESP supply current circulates on the
secondary and returns to the converter — it never flows through the Cell−
measurement tie. Consequences:

1. **True 4-wire sensing.** GND_MEAS stays at true cell-negative potential
   (only the INA's ~330 µA quiescent current is in it), so VBUS − GND is the
   real cell terminal voltage.
2. **Safe FET control.** GND_MEAS is bonded to Cell− independently of the power
   bank. When the bank's undervoltage protection trips (or the ESP hangs and
   the bank then trips on cell voltage), the FET gate (pull-down to GND_MEAS)
   and source (GND_PWR, also Cell−) stay at the same potential → FET off. No
   runaway deep-discharge.
3. **No opto needed.** Since ESP ground = GND_MEAS = the FET-source reference,
   the ESP GPIO drives the gate directly.

**The one rule:** GND_PWR and GND_MEAS join only at the cell − terminal
(single-point / star). Never bond them anywhere else, or the supply-current IR
drop re-enters the voltage reference and the 4-wire benefit is lost.

See the Mermaid diagram and net table in
[overview.md](overview.md#hardware-block-diagram).

## Hardware Tasks

1. **Wire INA226 to ESP32 via I2C**
   - SDA → GPIO 21 (default ESP32 I2C)
   - SCL → GPIO 22 (default ESP32 I2C)
   - INA226 VCC → 3.3V from ESP32
   - INA226 GND → **GND_MEAS** (the measurement domain, Kelvin to Cell−)
   - INA226 A0, A1 → GND (I2C address 0x40)

2. **Connect shunt resistor (100 mΩ) between IN+ and IN−**
   - IN+ goes to the cell positive terminal (Kelvin)
   - IN− goes to the common load node — both the discharge resistor network
     **and** the USB power bank input (B+) connect here
   - This places the shunt at the single point where all cell current flows,
     so the INA226 measures total cell current (discharge load + ESP supply)

3. **Connect VBUS for bus voltage measurement (4-wire)**
   - VBUS pin → cell positive terminal via a separate Kelvin sense wire
     (upstream of the shunt)
   - INA226 GND → cell negative terminal via the GND_MEAS Kelvin tie
   - VBUS − GND therefore reads the true cell terminal voltage, free of the
     shunt drop and free of supply-current IR drops

4. **Connect the single discharge path via an N-channel MOSFET**
   - FET gate → GPIO 25 (via 1kΩ gate resistor), drain → 13.5 Ω load,
     source → **GND_PWR** (heavy load-return wire to Cell−)
   - 10kΩ pull-down on the gate to GND_MEAS → FET stays off during ESP boot
     and whenever the ESP loses power or hangs (fail-safe)

5. **Power supply (isolated)**
   - USB power bank PCB between the cell and an isolated DC-DC:
     power-bank input (B+) → IN− node (downstream of the shunt), power-bank
     B− → GND_PWR
   - Power-bank 5V output → isolated DC-DC input
   - Isolated DC-DC output → ESP32 (5 V_iso / GND_MEAS)
   - This keeps the ESP supply current inside the measurement of the shunt
     (capacity includes ESP draw) while the isolation breaks the ground so the
     measurement side references true Cell−

## Software Tasks

1. **Create Arduino project skeleton**
   - `accucheck2.ino` with `setup()` and `loop()`
   - `config.h` with pin definitions and I2C address

2. **I2C scanner**
   - Scan I2C bus, print found addresses to Serial
   - Verify INA226 responds at 0x40

3. **Read INA226 registers**
   - Read manufacturer ID (register 0xFE, expect 0x5449)
   - Read die ID (register 0xFF, expect 0x2260)
   - Print to Serial to confirm communication

## Acceptance Criteria

- [x] INA226 detected on I2C bus at address 0x40
- [x] Manufacturer and die ID read correctly
- [x] Serial output shows register values
- [x] FET GPIO confirmed LOW at boot (no unwanted discharge)
- [ ] Isolated DC-DC in place; GND_PWR and GND_MEAS joined only at Cell−
- [ ] 4-wire cell-voltage reading confirmed (matches a DMM at the terminals)
- [ ] FET stays off when the power-bank protection trips / ESP loses power

## Resistor Value

Single discharge path: 2× 27Ω in parallel = 13.5Ω → ~274 mA at 3.7V nominal.
