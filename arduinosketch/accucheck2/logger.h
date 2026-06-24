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

#endif
