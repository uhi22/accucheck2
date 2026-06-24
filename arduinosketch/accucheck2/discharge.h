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
void dischargeTick();
DischargeState dischargeGetState();
const char* dischargeGetStateStr();
float dischargeGetCapacity_mAh();
float dischargeGetEnergy_mWh();
uint32_t dischargeGetElapsedSeconds();

#endif
