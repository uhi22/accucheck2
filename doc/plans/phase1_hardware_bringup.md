# Phase 1 — Hardware Setup & Basic I2C

## Goal

Wire up the ESP32 and INA226 on a breadboard, confirm I2C communication,
and read raw register values from the INA226.

## Hardware Tasks

1. **Wire INA226 to ESP32 via I2C**
   - SDA → GPIO 21 (default ESP32 I2C)
   - SCL → GPIO 22 (default ESP32 I2C)
   - INA226 VCC → 3.3V from ESP32
   - INA226 GND → common GND
   - INA226 A0, A1 → GND (I2C address 0x40)

2. **Connect shunt resistor (50 mΩ) between IN+ and IN-**
   - IN+ goes to the cell positive terminal
   - IN- goes to the common load node — both the discharge resistor network
     **and** the USB power bank input connect here
   - This places the shunt at the single point where all cell current flows,
     so the INA226 measures total cell current (discharge load + ESP32 supply)

3. **Connect VBUS for bus voltage measurement**
   - VBUS pin → cell positive terminal (separate Kelvin sense wire, on the IN+
     side, upstream of the shunt)
   - GND of INA226 is already on common GND
   - This gives the true cell terminal voltage (VBUS to GND), excluding the
     shunt's own voltage drop

4. **Connect two discharge paths via N-channel MOSFETs**
   - FET1 gate → GPIO 25 (via 1kΩ gate resistor), drain → R_low, source → GND
   - FET2 gate → GPIO 26 (via 1kΩ gate resistor), drain → R_high, source → GND
   - FETs off at startup (GPIOs default LOW)
   - 10kΩ pull-down on each gate to ensure FETs stay off during ESP32 boot

5. **Power supply**
   - USB power bank PCB between the cell and the ESP32
   - Power bank input → IN- node (downstream of the shunt, **not** directly to
     the cell), 5V output → ESP32 VIN
   - This routes the ESP32's supply current through the shunt so it is included
     in the measurement

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
- [x] FET GPIOs confirmed LOW at boot (no unwanted discharge)

## Resistor Value

Single discharge path: 2× 27Ω in parallel = 13.5Ω → ~274 mA at 3.7V nominal.
