#ifndef DCIR_H
#define DCIR_H

#include <stdint.h>

#define DCIR_MAX_SAMPLES 60

/* DCIR validity thresholds (tunable). The load step must produce enough current
   and voltage change to measure against noise, and the resulting R_i must be
   physically plausible. The upper R_i bound must be generous: weak/aged cells
   are exactly the high-resistance ones (often 0.5-several ohm), so a low ceiling
   silently drops the very cells we want to flag. */
#define DCIR_MIN_DI_MA      100.0f   /* min load-step current delta */
#define DCIR_MIN_DV_MV      1.0f     /* min load-step voltage drop. At the INA226
                                        bus-ADC resolution (1.25 mV/LSB), so R_i
                                        from such small drops is coarse, but it is
                                        better than dropping the point entirely. */
#define DCIR_RI_MIN_MOHM    1.0f     /* below this: implausible / noise */
#define DCIR_RI_MAX_MOHM    5000.0f  /* 5 ohm; covers very weak cells */

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
