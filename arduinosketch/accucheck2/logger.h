#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>

struct MeasurementData {
  uint32_t timestamp_s;
  int16_t voltage_mV;
  int16_t current_mA;
  float capacity_mAh;
  float energy_mWh;
  float ri_mOhm;
  const char* state;
};

void loggerInit();
void loggerTick();
bool loggerIsConnected();
void loggerSend(const MeasurementData &data);

/* Best-effort (not buffered) batch of high-speed DCIR samples for the detail
   chart. 'samples' is a comma-separated list of "t_ms:v:i" triples. */
void loggerSendDcirSamples(float ri_mOhm, uint32_t t_s, const char* samples);

#endif
