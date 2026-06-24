#ifndef LED_H
#define LED_H

/* Status LED feedback / firmware heartbeat.

   The LED is driven from the main loop via ledTick(). Because the toggle is
   time-driven, a frozen firmware shows a frozen LED, so the heartbeat doubles
   as a liveness indicator. */

enum LedMode {
  LED_IDLE,         /* slow blink (1 Hz) — waiting for cell / idle */
  LED_DISCHARGING,  /* short heartbeat pulse once per second — alive + running */
  LED_DCIR,         /* solid on — DCIR measurement in progress (blocking) */
  LED_DONE,         /* solid on — discharge finished */
  LED_ERROR         /* fast blink (5 Hz) — fault */
};

void ledInit();
void ledSetMode(LedMode mode);
void ledTick();

#endif
