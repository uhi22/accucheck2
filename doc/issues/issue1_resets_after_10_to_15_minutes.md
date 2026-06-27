# Issue: Resets after 10 to 15 minutes

## Description

An ESP32 DEVKIT, which sends each 10s some data via WiFi, runs after 8 to 15 minutes
into a dead loop. This causes the hardware watchdog to make a reset, and the game
starts again.

## Analysis

The most common root cause for this pattern is a memory leak / heap fragmentation in
the WiFi/networking code.

The key clue is the timing: 8–15 minutes with a transmission every 10s means roughly
 50–90 send cycles before the crash. If something small leaks each cycle, the heap
 eventually runs out or fragments to the point where an allocation fails or the network
 stack hangs, and the watchdog resets it.

Likely culprits:

Not freeing network objects each cycle — creating a new WiFiClient, HTTPClient,
WiFiClientSecure, or socket every loop and not closing/deleting it. 
TLS (WiFiClientSecure) is especially bad: each handshake grabs large buffers,
and leaked ones fragment the heap fast.
String concatenation in the loop — heavy use of Arduino String builds up
fragmentation over many iterations.
Blocking until a connection succeeds/fails without yielding — if client.connect() or
DNS hangs and you sit in a tight loop, the task watchdog fires. Missing delay()/yield()
starves the IDLE task that feeds the WDT.
Reconnect logic that stacks up — repeatedly calling WiFi.begin() or reconnecting without
cleaning up prior state.

How to confirm: log ESP.getFreeHeap() and ESP.getMaxAllocHeap() every cycle and
watch them over those 8–15 minutes. A steady downward trend in free heap, or a
collapsing max-alloc block while free heap looks okay (fragmentation), points
straight at the leak.

Also check the actual reset reason via esp_reset_reason() — distinguishing
task WDT (a task blocked too long) from interrupt WDT or a
panic/abort() (out-of-memory, null deref) narrows it down considerably. Posting that
reason plus a heap trace would let me pin it more precisely.

### With heap instrumentation

12:25:55.141 -> [heap] free=250324 minFree=247852 maxAlloc=110580 up_s=20
12:26:05.129 -> [heap] free=250324 minFree=247852 maxAlloc=110580 up_s=30
12:26:15.143 -> [heap] free=250324 minFree=247852 maxAlloc=110580 up_s=40
12:26:25.145 -> [heap] free=250324 minFree=247852 maxAlloc=110580 up_s=50
12:26:35.138 -> [heap] free=250324 minFree=247852 maxAlloc=110580 up_s=60
12:26:41.130 -> Cell detected, auto-starting...
12:26:41.130 -> Running DCIR...
12:26:43.768 -> R_i = 95.1 mOhm
12:26:43.851 -> Discharge started.
12:26:43.851 -> t_s	V_mV	I_mA	cap_mAh	e_mWh
12:26:43.889 -> 0	4003	328	0.0	0.0
12:26:45.140 -> [heap] free=248956 minFree=240860 maxAlloc=110580 up_s=70
12:26:53.881 -> 10	3927	608	1.3	5.1
12:26:55.115 -> [heap] free=248732 minFree=240860 maxAlloc=110580 up_s=80
12:27:03.894 -> 20	3921	611	3.0	11.8
12:27:05.129 -> [heap] free=248508 minFree=240860 maxAlloc=110580 up_s=90
12:27:13.867 -> 30	3913	612	4.7	18.4
12:27:15.130 -> [heap] free=248284 minFree=240860 maxAlloc=110580 up_s=100
12:27:23.889 -> 40	3911	604	6.4	25.0
12:27:25.116 -> [heap] free=248060 minFree=240860 maxAlloc=110580 up_s=110
12:27:33.881 -> 50	3907	618	8.1	31.7
...
12:28:36.641 -> [heap] free=246044 minFree=238844 maxAlloc=110580 up_s=181
12:28:43.883 -> 120	3886	620	20.1	78.6
12:28:43.946 -> Running DCIR...
12:28:46.565 -> R_i = 84.3 mOhm
12:28:46.642 -> [heap] free=245900 minFree=237696 maxAlloc=110580 up_s=191
12:28:53.890 -> 130	3886	621	21.8	85.3
12:28:56.613 -> [heap] free=246032 minFree=237696 maxAlloc=110580 up_s=201
12:29:03.876 -> 140	3880	620	23.6	92.0
12:29:06.645 -> [heap] free=246040 minFree=237696 maxAlloc=110580 up_s=211
12:29:13.881 -> 150	3877	621	25.3	98.7
12:29:16.640 -> [heap] free=246040 minFree=237696 maxAlloc=110580 up_s=221
12:29:23.891 -> 160	3876	620	27.0	105.4
12:29:26.646 -> [heap] free=246040 minFree=237696 maxAlloc=110580 up_s=231
12:29:33.882 -> 170	3872	623	28.7	112.0
12:29:36.651 -> [heap] free=246040 minFree=237696 maxAlloc=110580 up_s=241
12:29:43.881 -> 180	3868	623	30.5	118.7
12:29:46.609 -> [heap] free=246040 minFree=237696 maxAlloc=110580 up_s=251

...
12:31:33.881 -> 290	3837	624	49.5	192.2
12:31:36.650 -> [heap] free=246044 minFree=237696 maxAlloc=110580 up_s=361
12:31:43.887 -> 300	3833	625	51.3	198.8
12:32:02.890 -> HTTP error: -1
12:32:02.890 -> Running DCIR...
12:32:05.510 -> R_i = 92.5 mOhm
12:32:13.859 -> E (403978) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
12:32:13.932 -> E (403978) task_wdt:  - loopTask (CPU 1)
12:32:13.932 -> E (403978) task_wdt: Tasks currently running:
12:32:13.932 -> E (403978) task_wdt: CPU 0: IDLE0
12:32:13.932 -> E (403978) task_wdt: CPU 1: IDLE1
12:32:13.932 -> E (403978) task_wdt: Aborting.
12:32:13.932 -> E (403978) task_wdt: Print CPU 1 backtrace
12:32:13.932 -> 
12:32:13.932 -> 
12:32:13.932 -> 
12:32:13.932 -> 
12:32:13.932 -> Backtrace: 0x4008c433:0x3ffbcb90 0x400e712e:0x3ffbcbb0 0x4008ec8f:0x3ffbcbd0 0x4009081e:0x3ffbcbf0
12:32:13.932 -> 
12:32:13.932 -> 
12:32:13.932 -> 
12:32:13.932 -> 
12:32:13.932 -> ELF file SHA256: 8e7871aef91d925b

## Root cause (confirmed)

The instrumentation settled it: the original memory-leak / fragmentation theory was
wrong.

* Heap was healthy and flat right up to the crash (free=246044, maxAlloc=110580,
  no downward trend, no fragmentation).
* The reset reason was the **task watchdog** (`task_wdt ... loopTask (CPU 1)`),
  i.e. a genuine hang, not an out-of-memory panic.

Reconstructing the final loop() iteration:

```
12:31:43  300 ...            loggerSend(t=300) -> http.GET() begins
12:32:02  HTTP error: -1     that GET blocked ~19 s, then failed to connect
12:32:02  Running DCIR...     periodic DCIR runs in the SAME iteration
12:32:05  R_i = 92.5 mOhm     dcirMeasure ~3 s, then tries to send samples too
12:32:13  task_wdt triggered  30 s with no feed -> reset
```

The server (hnng.de) became briefly unreachable while WiFi stayed associated
(note: no "WiFi reconnecting" message). The real bug: `http.setTimeout()` only
caps the stream read, **not** the TCP connect. With no connect timeout, each
`http.GET()` blocked ~19 s. Several such blocking calls stack inside one loop()
iteration (the immediate measurement send + the DCIR-samples send), and since
`watchdogFeed()` only ran at the top of loop(), the 30 s task watchdog fired.

## Fix

Defense-in-depth so no single loop() iteration can exceed the 30 s watchdog:

1. `http.setConnectTimeout(HTTP_TIMEOUT_MS)` in addition to `setTimeout()` — caps
   each GET at ~5 s instead of ~19 s.
2. `watchdogFeed()` after every HTTP request — stacked slow sends each refresh the
   timer (slow progress no longer looks like a dead loop).
3. Skip the best-effort DCIR-samples send when a backlog exists (`bufCount > 0`) —
   avoids a second long block right after a failed send.
4. (Earlier hardening) the buffer-flush loop feeds the watchdog and is capped to
   FLUSH_MAX_PER_TICK sends per tick.

## Diagnostics instrumentation (kept in permanently)

Added to pin the root cause, and left in for future debugging:

* `printResetReason()` at boot prints the decoded `esp_reset_reason()` so every
  reboot states why it happened (TASK_WDT vs PANIC vs BROWNOUT vs POWERON ...).
  This is what disproved the heap theory and confirmed the watchdog hang.
* `heapMonitorTick()` logs `free / minFree / maxAlloc / uptime` once per cycle:
  `[heap] free=... minFree=... maxAlloc=... up_s=...`. A downward `free` trend
  would mean a leak; a collapsing `maxAlloc` would mean fragmentation. Both were
  flat, exonerating memory.

## Connectivity-health telemetry (warning indicator)

So network instability is visible early instead of only after a crash, the device
now sends a compact diagnostic record to the server every 60 s, and the website
shows it on a dedicated page.

* Firmware: `loggerSendDiagnostics()` sends `diag=1` with `up` (uptime), `rssi`,
  `heap`, `minheap`, cumulative `ok`/`err` HTTP counts, `reconn` (WiFi reconnects)
  and `reassoc` (forced re-associations), plus the boot `reset` reason. Best-effort,
  not buffered, same connect-timeout + watchdog-feed hardening as the data sends.
* Server: `add.php` stores `diag=1` requests in `data/diag.txt`, stamped with the
  server's wall-clock `recv_time` so the record survives device reboots (which
  reset the uptime). `data.php?diaglog=1` returns it as JSON.
* Website: `diag.php` (linked from `index.php`) charts RSSI, heap, and cumulative
  HTTP ok/err, and lists recent records. Rows where the device had just rebooted
  for a bad reason (TASK_WDT / BROWNOUT / PANIC with uptime < 120 s) are highlighted.

This page doubles as the fix verifier: if the fix holds, `err` may climb during an
outage while uptime keeps growing (no reset); a regression shows up as a
highlighted row with the exact reset reason and the RSSI/heap context around it.

## Files changed

* `arduinosketch/accucheck2/accucheck2.ino` — reset-reason print + token, heap monitor.
* `arduinosketch/accucheck2/logger.cpp` / `logger.h` — connect timeout, watchdog
  feeds after every HTTP request, flush cap, backlog skip, health counters and the
  periodic diagnostic record.
* `webspace/add.php` — store `diag` records in `data/diag.txt`.
* `webspace/data.php` — `diaglog` endpoint.
* `webspace/diag.php` — new connectivity-diagnostics page; linked from `index.php`.

## Verification

1. Heap trace stayed flat and the reset reason confirmed `TASK_WDT` (above), ruling
   out the memory hypothesis and confirming the hang.
2. To exercise the fixed path on the bench: with WiFi associated, make the server
   (hnng.de:80) unreachable for ~1-2 min, then restore. Expected with the fix:
   repeated `HTTP error: -1` spaced ~5 s apart (not ~19 s), `err`/`reconn`/`reassoc`
   rising on `diag.php`, the backlog flushing on recovery, and **no reset**
   (uptime keeps increasing).