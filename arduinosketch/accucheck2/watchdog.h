#ifndef WATCHDOG_H
#define WATCHDOG_H

/* Hardware task watchdog (ESP32 TWDT).

   If the firmware hangs (deadlock, I2C lock-up, infinite loop) the loop task
   stops feeding the watchdog and the chip resets after the timeout — even
   though no software is running anymore. This is the backstop the software
   connectivity-watchdog cannot provide (that one only works while loop() runs).

   The timeout must be longer than the longest legitimate blocking section
   (DCIR ~3 s, HTTP 5 s timeout) so it never false-triggers during normal work.
   A reset turns the discharge FET off via its gate pull-down. */

void watchdogInit();   /* arm the TWDT and subscribe the loop task */
void watchdogFeed();    /* call once per loop() iteration */

#endif
