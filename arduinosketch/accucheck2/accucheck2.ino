/* Board: ESP32 DEVKIT V1 */

#include <Wire.h>
#include "config.h"
#include "ina226.h"
#include "discharge.h"
#include "dcir.h"
#include "logger.h"
#include "led.h"
#include "watchdog.h"

static float lastRI_mOhm = 0;
static unsigned long lastDcirTime_ms = 0;
static unsigned long autoStartCheckTime_ms = 0;

#define DCIR_INTERVAL_MS   60000    /* 1 minute */
#define AUTOSTART_SETTLE_MS 3000    /* voltage must be stable for 3s */
#define AUTOSTART_V_MIN    3000     /* mV */
#define AUTOSTART_V_MAX    4250     /* mV */

static int16_t autoStartVoltage = 0;
static unsigned long autoStartStableTime = 0;
static bool autoStartEnabled = true;

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

  ledInit();
  dischargeInit();
  loggerInit();

  /* Arm last, after the blocking WiFi connect in loggerInit(), so the connect
     loop does not trip the watchdog. */
  watchdogInit();

  Serial.println("READY");
  Serial.println("Commands: start / stop / status / reset / dcir / hang");
}

static String cmdBuffer;

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd == "start") {
    dischargeStart();
    lastDcirTime_ms = millis();
    Serial.println("t_s\tV_mV\tI_mA\tcap_mAh\te_mWh");
  } else if (cmd == "stop") {
    dischargeStop();
    autoStartEnabled = false;
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
    ledSetMode(LED_DCIR);  /* solid on during the blocking measurement */
    DcirResult result;
    dcirMeasure(result);
    if (result.valid) {
      lastRI_mOhm = result.ri_mOhm;
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
  } else if (cmd == "hang") {
    /* Test hook: deliberately hang the loop to verify the hardware watchdog
       resets the device after the TWDT timeout. */
    Serial.println("Hanging on purpose - watchdog should reset in ~30 s...");
    Serial.flush();
    while (true) { /* feed nothing, run nothing */ }
  } else if (cmd == "reset") {
    dischargeInit();
    autoStartEnabled = false;
    Serial.println("Reset to IDLE.");
  } else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
  }
}

static void runDcirAndLog() {
  Serial.println("Running DCIR...");
  ledSetMode(LED_DCIR);  /* solid on during the blocking measurement */
  DcirResult result;
  dcirMeasure(result);
  if (result.valid) {
    lastRI_mOhm = result.ri_mOhm;
    Serial.print("R_i = ");
    Serial.print(result.ri_mOhm, 1);
    Serial.println(" mOhm");

    /* Log a dedicated "dcir" data point so every DCIR (including the initial
       one at auto-start, before discharge begins) shows up on the website.
       Use the DCIR's own loaded readings so the initial point is not (0,0). */
    MeasurementData dcirData;
    dcirData.timestamp_s = dischargeGetElapsedSeconds();
    dcirData.voltage_mV = (int16_t)result.v_load_mV;
    dcirData.current_mA = (int16_t)result.i_load_mA;
    dcirData.capacity_mAh = dischargeGetCapacity_mAh();
    dcirData.energy_mWh = dischargeGetEnergy_mWh();
    dcirData.ri_mOhm = lastRI_mOhm;
    dcirData.state = "dcir";
    loggerSend(dcirData);
  } else {
    Serial.println("DCIR measurement failed.");
  }
}

static void checkAutoStart() {
  if (!autoStartEnabled) return;
  unsigned long now = millis();
  if (now - autoStartCheckTime_ms < 1000) return;
  autoStartCheckTime_ms = now;

  int16_t v = ina226ReadBusVoltage_mV();
  if (v >= AUTOSTART_V_MIN && v <= AUTOSTART_V_MAX) {
    if (autoStartVoltage == 0 || abs(v - autoStartVoltage) > 50) {
      autoStartVoltage = v;
      autoStartStableTime = now;
    } else if (now - autoStartStableTime >= AUTOSTART_SETTLE_MS) {
      Serial.println("Cell detected, auto-starting...");
      runDcirAndLog();
      dischargeStart();
      Serial.println("t_s\tV_mV\tI_mA\tcap_mAh\te_mWh");
      lastDcirTime_ms = millis();
    }
  } else {
    autoStartVoltage = 0;
    autoStartStableTime = 0;
  }
}

void loop() {
  watchdogFeed();

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

  DischargeState ds = dischargeGetState();
  if (ds == STATE_IDLE) {
    checkAutoStart();
  } else if (ds == STATE_DONE || ds == STATE_ERROR) {
    autoStartEnabled = false;
  }

  /* Status LED follows the discharge state (LED_DCIR is set around the
     blocking DCIR measurement inside runDcirAndLog) */
  switch (ds) {
    case STATE_IDLE:        ledSetMode(LED_IDLE);        break;
    case STATE_DISCHARGING: ledSetMode(LED_DISCHARGING); break;
    case STATE_DONE:        ledSetMode(LED_DONE);        break;
    case STATE_ERROR:       ledSetMode(LED_ERROR);       break;
  }

  if (dischargeTick()) {
    MeasurementData data;
    data.timestamp_s = dischargeGetElapsedSeconds();
    data.voltage_mV = dischargeGetLastVoltage_mV();
    data.current_mA = dischargeGetLastCurrent_mA();
    data.capacity_mAh = dischargeGetCapacity_mAh();
    data.energy_mWh = dischargeGetEnergy_mWh();
    data.ri_mOhm = 0; /* only sent in dedicated "dcir" data points, to avoid repeating stale values */
    data.state = dischargeGetStateStr();
    loggerSend(data);

    /* Periodic DCIR every 2 minutes during discharge (runDcirAndLog logs the
       data point itself) */
    unsigned long now = millis();
    if (now - lastDcirTime_ms >= DCIR_INTERVAL_MS) {
      lastDcirTime_ms = now;
      runDcirAndLog();
      /* Re-enable FET after DCIR (dcirMeasure leaves it off) */
      digitalWrite(FET_PIN, HIGH);
    }
  }

  loggerTick();
  ledTick();
}
