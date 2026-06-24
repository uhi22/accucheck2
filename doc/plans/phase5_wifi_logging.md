# Phase 5 — WiFi & HTTP Logging

## Goal

Connect the ESP32 to WiFi and send measurement data as HTTP GET requests
to an external server, with parameters in the URL.

## HTTP GET Format

```
http://<server>:<port>/accucheck2/add.php?t=<timestamp>&v=<voltage_mV>&i=<current_mA>&cap=<capacity_mAh>&ri=<dcir_mOhm>&state=<state>
```

### Parameters

| Param | Description | Unit | Example |
|---|---|---|---|
| t | Seconds since test start | s | 1200 |
| v | Cell voltage | mV | 3742 |
| i | Discharge current | mA | 985 |
| cap | Accumulated capacity | mAh | 328 |
| ri | Last DCIR result | mΩ | 42 |
| state | Current state | string | discharging |
| e | Accumulated energy | mWh | 1215 |

### States

`idle`, `discharging`, `dcir`, `done`, `error`

## WiFi Configuration

Stored in `config.h`:

```cpp
#define WIFI_SSID     "your_ssid"
#define WIFI_PASSWORD "your_password"
#define SERVER_HOST   "192.168.1.100"
#define SERVER_PORT   8080
#define SERVER_PATH   "/accucheck2/yourapi.php"
```

## Software Tasks

1. **Logger module** (`logger.h` / `logger.cpp`)
   - `initWiFi()` — connect to WiFi, retry with backoff
   - `sendMeasurement(data)` — build URL, send HTTP GET, check response
   - `isConnected()` — WiFi status check
   - Non-blocking: if WiFi is down, buffer last N measurements and retry

2. **WiFi connection management**
   - Connect in `setup()`
   - Auto-reconnect if connection drops
   - Measurement continues even without WiFi (data logged to Serial as fallback)
   - LED or Serial indicator for WiFi status

3. **HTTP client**
   - Use `HTTPClient` from Arduino ESP32 library
   - Timeout: 5 seconds per request
   - Fire-and-forget: don't block measurement loop waiting for response
   - Log HTTP response code to Serial for debugging

4. **Buffering (simple)**
   - Ring buffer for last 10 unsent measurements
   - On successful connection, flush buffer
   - If buffer full, overwrite oldest entry

5. **Connectivity watchdog**
   - Tracks time since the last successful HTTP 200, but only escalates while
     there is undelivered data buffered (so an idle device with no WiFi does not
     reboot-loop)
   - After ~30 s of persistent failure (covers the recurring `HTTP error: -1`,
     where WiFi is associated but the socket is dead): force a WiFi
     re-association as an on-the-fly fix
   - After ~120 s still failing: `ESP.restart()` — this stops the discharge via
     the FET pull-down and starts a fresh attempt (and a fresh log file) on the
     next boot

## Server Side

The PHP server implementation is covered in Phase 6 (see [phase6_server.md](phase6_server.md)).
For initial testing of the ESP32 WiFi code before the PHP backend is ready,
the Serial output can be used to verify that the HTTP requests are being built correctly.

## Acceptance Criteria

- [x] ESP32 connects to WiFi within 10 seconds
- [x] HTTP GET received by server with correct parameters
- [x] Measurement loop not blocked by slow/failed HTTP requests (ring buffer, non-blocking)
- [x] Data still logged to Serial when WiFi is unavailable
- [ ] Auto-reconnect works after WiFi dropout
