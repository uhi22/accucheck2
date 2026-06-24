#ifndef DISCHARGE_H
#define DISCHARGE_H

#include <stdint.h>

enum DischargeState {
  STATE_IDLE,
  STATE_DISCHARGING,
  STATE_DONE,
  STATE_ERROR
};

void dischargeInit();
void dischargeStart();
void dischargeStop();
bool dischargeTick();
DischargeState dischargeGetState();
const char* dischargeGetStateStr();
float dischargeGetCapacity_mAh();
float dischargeGetEnergy_mWh();
uint32_t dischargeGetElapsedSeconds();
int16_t dischargeGetLastVoltage_mV();
int16_t dischargeGetLastCurrent_mA();

#endif
