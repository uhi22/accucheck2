#include <Arduino.h>
#include "esp_task_wdt.h"
#include "watchdog.h"

/* Comfortably above the longest blocking section (DCIR ~3 s, HTTP 5 s, plus a
   periodic-DCIR loop iteration ~18 s worst case). The power-bank undervoltage
   protection is the ultimate backstop against over-discharge if the FET stays
   on during the hang window. */
#define WDT_TIMEOUT_S 30

void watchdogInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  /* Core 3.x: the Arduino core already initializes the TWDT (for the idle
     tasks), so reconfigure its timeout and subscribe the loop task. */
  esp_task_wdt_config_t cfg = {
    .timeout_ms = (uint32_t)(WDT_TIMEOUT_S * 1000),
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&cfg);
  esp_task_wdt_add(NULL);
#else
  /* Core 2.x */
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
#endif
  Serial.print("Task watchdog armed: ");
  Serial.print(WDT_TIMEOUT_S);
  Serial.println(" s");
}

void watchdogFeed() {
  esp_task_wdt_reset();
}
