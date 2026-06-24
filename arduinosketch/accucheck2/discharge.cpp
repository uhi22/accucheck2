#include <Arduino.h>
#include "config.h"
#include "ina226.h"
#include "discharge.h"

static DischargeState state = STATE_IDLE;
static float capacity_mAh = 0;
static float energy_mWh = 0;
static int16_t prevCurrent_mA = 0;
static int16_t prevVoltage_mV = 0;
static unsigned long startTime_ms = 0;
static unsigned long prevTime_ms = 0;
static unsigned long lastTick_ms = 0;

static void fetOn() {
  digitalWrite(FET_PIN, HIGH);
}

static void fetOff() {
  digitalWrite(FET_PIN, LOW);
}

void dischargeInit() {
  fetOff();
  state = STATE_IDLE;
}

void dischargeStart() {
  if (state == STATE_DISCHARGING) return;
  capacity_mAh = 0;
  energy_mWh = 0;
  prevCurrent_mA = 0;
  prevVoltage_mV = 0;
  startTime_ms = millis();
  prevTime_ms = startTime_ms;
  lastTick_ms = 0;
  fetOn();
  state = STATE_DISCHARGING;
  Serial.println("Discharge started.");
}

void dischargeStop() {
  fetOff();
  if (state == STATE_DISCHARGING) {
    state = STATE_IDLE;
    Serial.println("Discharge stopped by user.");
    Serial.print("Capacity: ");
    Serial.print(capacity_mAh, 1);
    Serial.print(" mAh, Energy: ");
    Serial.print(energy_mWh, 1);
    Serial.print(" mWh, Time: ");
    Serial.print(dischargeGetElapsedSeconds());
    Serial.println(" s");
  }
}

static void finishDone() {
  fetOff();
  state = STATE_DONE;
  Serial.println("Discharge complete (cutoff reached).");
  Serial.print("Capacity: ");
  Serial.print(capacity_mAh, 1);
  Serial.print(" mAh, Energy: ");
  Serial.print(energy_mWh, 1);
  Serial.print(" mWh, Time: ");
  Serial.print(dischargeGetElapsedSeconds());
  Serial.println(" s");
}

static void finishError(const char* reason) {
  fetOff();
  state = STATE_ERROR;
  Serial.print("Discharge ERROR: ");
  Serial.println(reason);
}

void dischargeTick() {
  if (state != STATE_DISCHARGING) return;

  unsigned long now = millis();
  if (lastTick_ms != 0 && (now - lastTick_ms) < MEASURE_INTERVAL_MS) return;
  lastTick_ms = now;

  if (!ina226IsConversionReady()) return;

  int16_t voltage = ina226ReadBusVoltage_mV();
  int16_t current = ina226ReadCurrent_mA();

  // Safety checks
  if (voltage == 0) {
    finishError("voltage 0 - cell disconnected?");
    return;
  }
  if (current > 2000) {
    finishError("overcurrent");
    return;
  }
  uint32_t elapsed = (now - startTime_ms) / 1000;
  if (elapsed > MAX_DISCHARGE_TIME_S) {
    finishError("max discharge time exceeded");
    return;
  }

  // Integrate capacity and energy (trapezoidal)
  if (prevTime_ms != startTime_ms || prevCurrent_mA != 0) {
    float dt_hours = (now - prevTime_ms) / 3600000.0f;
    capacity_mAh += (prevCurrent_mA + current) / 2.0f * dt_hours;
    float prevPower = (float)prevVoltage_mV * prevCurrent_mA / 1000.0f;
    float curPower = (float)voltage * current / 1000.0f;
    energy_mWh += (prevPower + curPower) / 2.0f * dt_hours;
  }

  prevCurrent_mA = current;
  prevVoltage_mV = voltage;
  prevTime_ms = now;

  // Serial report
  Serial.print(elapsed);
  Serial.print("\t");
  Serial.print(voltage);
  Serial.print("\t");
  Serial.print(current);
  Serial.print("\t");
  Serial.print(capacity_mAh, 1);
  Serial.print("\t");
  Serial.println(energy_mWh, 1);

  // Cutoff check (after reporting, so we see the last value)
  if (voltage < V_CUTOFF_MV) {
    finishDone();
  }
}

DischargeState dischargeGetState() {
  return state;
}

const char* dischargeGetStateStr() {
  switch (state) {
    case STATE_IDLE:        return "idle";
    case STATE_DISCHARGING: return "discharging";
    case STATE_DONE:        return "done";
    case STATE_ERROR:       return "error";
    default:                return "unknown";
  }
}

float dischargeGetCapacity_mAh() {
  return capacity_mAh;
}

float dischargeGetEnergy_mWh() {
  return energy_mWh;
}

uint32_t dischargeGetElapsedSeconds() {
  if (state == STATE_IDLE && startTime_ms == 0) return 0;
  return (millis() - startTime_ms) / 1000;
}
