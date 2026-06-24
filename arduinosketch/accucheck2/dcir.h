#ifndef DCIR_H
#define DCIR_H

#include <stdint.h>

#define DCIR_MAX_SAMPLES 60

struct DcirSample {
  uint16_t time_ms;
  float voltage_mV;
  float current_mA;
};

struct DcirResult {
  float ri_mOhm;
  float v_rest_mV;
  float i_rest_mA;
  float v_load_mV;
  float i_load_mA;
  bool valid;
  DcirSample samples[DCIR_MAX_SAMPLES];
  uint8_t sampleCount;
};

void dcirMeasure(DcirResult &result);

#endif
