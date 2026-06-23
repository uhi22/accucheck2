# Concept for accucheck2

## Goals:

- measure capacity and internal resistance of single LiIon battery cell
- no charging, only discharging
- powered by the cell under test
- logging to http endpoint via WiFi

## Measuring method for R_i: DC Internal Resistance (DCIR) — Load Step Test

Principle: Apply a known current pulse and measure the voltage drop.

Combined with Four-Wire measurement, to eliminate the effect of contact resistances

## Hardware Concept

- ESP32 for I2C, GPIOs, WiFi
- INA226 for measurement of cell voltage and cell current (via shunt). 16 bit resolution. I2C connection to the ESP32
- multiple discharge resistors, controlled via n-channel-FETs, to allow different discharge currents
- typical discharge current 1A, selectable also 0.5A
- optional: NTC for cell temperature measurement
- supply with PCB of an USB power bank. Includes cell-to-5V-step-up, undervoltage shutdown, shortcut protection
