/* Board: ESP32 DEVKIT V1 */

#include <Wire.h>
#include "config.h"

void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  Serial.print("Scan complete. Devices found: ");
  Serial.println(found);
}

uint16_t ina226ReadRegister(uint8_t reg) {
  Wire.beginTransmission(INA226_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)INA226_ADDRESS, (uint8_t)2);
  uint16_t val = (Wire.read() << 8) | Wire.read();
  return val;
}

void checkINA226() {
  uint16_t manufacturerID = ina226ReadRegister(0xFE);
  uint16_t dieID = ina226ReadRegister(0xFF);

  Serial.print("Manufacturer ID: 0x");
  Serial.print(manufacturerID, HEX);
  Serial.println(manufacturerID == 0x5449 ? " (OK)" : " (UNEXPECTED)");

  Serial.print("Die ID:          0x");
  Serial.print(dieID, HEX);
  Serial.println(dieID == 0x2260 ? " (OK)" : " (UNEXPECTED)");
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("accucheck2 starting...");

  pinMode(FET_LOW_PIN, OUTPUT);
  pinMode(FET_HIGH_PIN, OUTPUT);
  digitalWrite(FET_LOW_PIN, LOW);
  digitalWrite(FET_HIGH_PIN, LOW);
  Serial.println("FET GPIOs initialized LOW.");

  Wire.begin(I2C_SDA, I2C_SCL);
  scanI2C();
  checkINA226();

  Serial.println("READY");
}

void loop() {
}
