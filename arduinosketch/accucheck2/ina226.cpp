#include <Wire.h>
#include "config.h"
#include "ina226.h"

#define REG_CONFIG      0x00
#define REG_SHUNT_V     0x01
#define REG_BUS_V       0x02
#define REG_POWER       0x03
#define REG_CURRENT     0x04
#define REG_CALIBRATION 0x05
#define REG_MASK_ENABLE 0x06

// Config register bits:
// Averaging 64 samples = 011 (bits 11:9)
// Bus voltage conversion 1.1ms = 100 (bits 8:6)
// Shunt voltage conversion 1.1ms = 100 (bits 5:3)
// Mode: continuous shunt + bus = 111 (bits 2:0)
#define CONFIG_NORMAL 0x4127  // 0b0100_0001_0010_0111

// Fast config for DCIR:
// Averaging 16 samples = 010 (bits 11:9)
// Bus voltage conversion 332µs = 010 (bits 8:6)
// Shunt voltage conversion 332µs = 010 (bits 5:3)
// Mode: continuous shunt + bus = 111 (bits 2:0)
#define CONFIG_FAST 0x4497  // 0b0100_0100_1001_0111

// CAL = 0.00512 / (current_LSB * R_shunt)
// current_LSB = 50 µA = 0.00005 A
// R_shunt = 100 mΩ = 0.1 Ω
// CAL = 0.00512 / (0.00005 * 0.1) = 1024
#define CAL_VALUE 1024

static void writeRegister(uint8_t reg, uint16_t val) {
  Wire.beginTransmission(INA226_ADDRESS);
  Wire.write(reg);
  Wire.write((val >> 8) & 0xFF);
  Wire.write(val & 0xFF);
  Wire.endTransmission();
}

static uint16_t readRegister(uint8_t reg) {
  Wire.beginTransmission(INA226_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)INA226_ADDRESS, (uint8_t)2);
  uint16_t val = (Wire.read() << 8) | Wire.read();
  return val;
}

void ina226Init() {
  writeRegister(REG_CONFIG, CONFIG_NORMAL);
  writeRegister(REG_CALIBRATION, CAL_VALUE);
}

void ina226ConfigFast() {
  writeRegister(REG_CONFIG, CONFIG_FAST);
}

void ina226ConfigNormal() {
  writeRegister(REG_CONFIG, CONFIG_NORMAL);
}

int16_t ina226ReadBusVoltage_mV() {
  // Bus voltage register: unsigned, LSB = 1.25 mV
  uint16_t raw = readRegister(REG_BUS_V);
  return (int16_t)((raw * 125L) / 100);
}

float ina226ReadBusVoltage_mV_f() {
  uint16_t raw = readRegister(REG_BUS_V);
  return raw * 1.25f;
}

int16_t ina226ReadCurrent_mA() {
  // Current register: signed, LSB = current_LSB = 50 µA = 0.05 mA
  int16_t raw = (int16_t)readRegister(REG_CURRENT);
  return (int16_t)((raw * 50L) / 1000);
}

float ina226ReadCurrent_mA_f() {
  int16_t raw = (int16_t)readRegister(REG_CURRENT);
  return raw * 0.05f;
}

int16_t ina226ReadPower_mW() {
  // Power register: unsigned, LSB = 25 * current_LSB = 25 * 50 µA = 1.25 mW
  uint16_t raw = readRegister(REG_POWER);
  return (int16_t)((raw * 125L) / 100);
}

bool ina226IsConversionReady() {
  uint16_t mask = readRegister(REG_MASK_ENABLE);
  return (mask & 0x0008) != 0;  // CVRF bit
}
