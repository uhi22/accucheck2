#ifndef INA226_H
#define INA226_H

#include <stdint.h>

void ina226Init();
void ina226ConfigFast();
void ina226ConfigNormal();
int16_t ina226ReadBusVoltage_mV();
float ina226ReadBusVoltage_mV_f();
int16_t ina226ReadCurrent_mA();
float ina226ReadCurrent_mA_f();
int16_t ina226ReadPower_mW();
bool ina226IsConversionReady();

#endif
