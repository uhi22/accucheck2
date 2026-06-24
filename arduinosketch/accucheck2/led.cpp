#include <Arduino.h>
#include "config.h"
#include "led.h"

static LedMode currentMode = LED_IDLE;
static unsigned long modeStart = 0;

/* On/off times per mode in ms. A solid mode uses offMs = 0 (always on). */
static void modeTiming(LedMode mode, unsigned long &onMs, unsigned long &offMs) {
  switch (mode) {
    case LED_IDLE:        onMs = 500; offMs = 500; break;  /* 1 Hz */
    case LED_DISCHARGING: onMs = 30;  offMs = 970; break;  /* short heartbeat */
    case LED_DCIR:        onMs = 1;   offMs = 0;   break;  /* solid on */
    case LED_DONE:        onMs = 1;   offMs = 0;   break;  /* solid on */
    case LED_ERROR:       onMs = 100; offMs = 100; break;  /* 5 Hz */
    default:              onMs = 500; offMs = 500; break;
  }
}

/* Compute and apply the LED level for the active mode at time 'now'. */
static void applyLevel(unsigned long now) {
  unsigned long onMs, offMs;
  modeTiming(currentMode, onMs, offMs);
  unsigned long period = onMs + offMs;
  bool on;
  if (period == 0) {
    on = true;
  } else {
    unsigned long phase = (now - modeStart) % period;
    on = (phase < onMs);
  }
  digitalWrite(LED_PIN, on ? HIGH : LOW);  /* onboard LED is active high */
}

void ledInit() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  currentMode = LED_IDLE;
  modeStart = millis();
}

void ledSetMode(LedMode mode) {
  if (mode == currentMode) return;  /* keep blink phase running */
  currentMode = mode;
  modeStart = millis();
  applyLevel(modeStart);  /* apply immediately so solid modes show during blocking calls */
}

void ledTick() {
  applyLevel(millis());
}
