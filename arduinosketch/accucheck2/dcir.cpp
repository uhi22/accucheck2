#include <Arduino.h>
#include "config.h"
#include "ina226.h"
#include "dcir.h"

static void collectSamples(DcirResult &result, uint8_t count, unsigned long t0) {
  for (uint8_t i = 0; i < count && result.sampleCount < DCIR_MAX_SAMPLES; i++) {
    while (!ina226IsConversionReady()) {}
    DcirSample &s = result.samples[result.sampleCount];
    s.time_ms = (uint16_t)(millis() - t0);
    s.voltage_mV = ina226ReadBusVoltage_mV_f();
    s.current_mA = ina226ReadCurrent_mA_f();
    result.sampleCount++;
  }
}

static float avgVoltage(DcirResult &result, uint8_t fromEnd, uint8_t count) {
  float sum = 0;
  uint8_t start = result.sampleCount - fromEnd;
  for (uint8_t i = start; i < start + count; i++) {
    sum += result.samples[i].voltage_mV;
  }
  return sum / count;
}

static float avgCurrent(DcirResult &result, uint8_t fromEnd, uint8_t count) {
  float sum = 0;
  uint8_t start = result.sampleCount - fromEnd;
  for (uint8_t i = start; i < start + count; i++) {
    sum += result.samples[i].current_mA;
  }
  return sum / count;
}

void dcirMeasure(DcirResult &result) {
  result.valid = false;
  result.sampleCount = 0;
  result.ri_mOhm = 0;

  digitalWrite(FET_PIN, LOW);
  ina226ConfigFast();
  delay(20);
  delay(2000);

  unsigned long t0 = millis();

  collectSamples(result, 20, t0);
  result.v_rest_mV = avgVoltage(result, 5, 5);
  result.i_rest_mA = avgCurrent(result, 5, 5);

  digitalWrite(FET_PIN, HIGH);
  collectSamples(result, 20, t0);
  result.v_load_mV = avgVoltage(result, 5, 5);
  result.i_load_mA = avgCurrent(result, 5, 5);

  digitalWrite(FET_PIN, LOW);
  collectSamples(result, 20, t0);

  ina226ConfigNormal();

  float dV = result.v_rest_mV - result.v_load_mV;
  float dI = result.i_load_mA - result.i_rest_mA;

  if (dI < 100.0f) {
    Serial.println("DCIR: current delta too small (<100 mA)");
    return;
  }
  if (dV < 5.0f) {
    Serial.println("DCIR: voltage drop too small (<5 mV)");
    return;
  }

  result.ri_mOhm = dV * 1000.0f / dI;

  if (result.ri_mOhm < 10.0f || result.ri_mOhm > 500.0f) {
    Serial.print("DCIR: result out of plausible range: ");
    Serial.print(result.ri_mOhm, 1);
    Serial.println(" mOhm");
    return;
  }

  result.valid = true;
}
