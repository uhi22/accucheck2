/* Board: ESP32 DEVKIT V1 */

#include <Wire.h>
#include "config.h"
#include "ina226.h"
#include "discharge.h"
#include "dcir.h"

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

  pinMode(FET_PIN, OUTPUT);
  digitalWrite(FET_PIN, LOW);
  Serial.println("FET GPIO initialized LOW.");

  Wire.begin(I2C_SDA, I2C_SCL);
  scanI2C();
  checkINA226();
  ina226Init();
  Serial.println("INA226 configured.");

  dischargeInit();

  Serial.println("READY");
  Serial.println("Commands: start / stop / status / reset / dcir");
}

static String cmdBuffer;

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd == "start") {
    dischargeStart();
    Serial.println("t_s\tV_mV\tI_mA\tcap_mAh\te_mWh");
  } else if (cmd == "stop") {
    dischargeStop();
  } else if (cmd == "status") {
    Serial.print("State: ");
    Serial.println(dischargeGetStateStr());
    Serial.print("V=");
    Serial.print(ina226ReadBusVoltage_mV());
    Serial.print(" mV, I=");
    Serial.print(ina226ReadCurrent_mA());
    Serial.print(" mA, Cap=");
    Serial.print(dischargeGetCapacity_mAh(), 1);
    Serial.print(" mAh, E=");
    Serial.print(dischargeGetEnergy_mWh(), 1);
    Serial.print(" mWh, t=");
    Serial.print(dischargeGetElapsedSeconds());
    Serial.println(" s");
  } else if (cmd == "dcir") {
    Serial.println("Running DCIR measurement...");
    DcirResult result;
    dcirMeasure(result);
    if (result.valid) {
      Serial.print("R_i = ");
      Serial.print(result.ri_mOhm, 1);
      Serial.println(" mOhm");
      Serial.print("V_rest=");
      Serial.print(result.v_rest_mV, 2);
      Serial.print(" mV, I_rest=");
      Serial.print(result.i_rest_mA, 2);
      Serial.print(" mA, V_load=");
      Serial.print(result.v_load_mV, 2);
      Serial.print(" mV, I_load=");
      Serial.print(result.i_load_mA, 2);
      Serial.println(" mA");
      Serial.println("Samples (t_ms, V_mV, I_mA):");
      for (uint8_t i = 0; i < result.sampleCount; i++) {
        Serial.print(result.samples[i].time_ms);
        Serial.print("\t");
        Serial.print(result.samples[i].voltage_mV, 2);
        Serial.print("\t");
        Serial.println(result.samples[i].current_mA, 2);
      }
    } else {
      Serial.println("DCIR measurement failed.");
    }
  } else if (cmd == "reset") {
    dischargeInit();
    Serial.println("Reset to IDLE.");
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdBuffer.length() > 0) {
        handleCommand(cmdBuffer);
        cmdBuffer = "";
      }
    } else {
      cmdBuffer += c;
    }
  }

  dischargeTick();
}
