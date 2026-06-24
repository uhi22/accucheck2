# Phase 6 — PHP Server & Visualization

## Goal

Build a PHP backend on a real web space that receives measurement data from the ESP32
via HTTP GET, stores it line-by-line in text files, and serves a visualization page
with live-updating charts.

## Server Components

```
Web Space (PHP)
  ├── add.php        ← receives GET from ESP32, appends to log file
  ├── data.php             ← returns log data as JSON for the chart page
  ├── index.php           ← visualization page (Chart.js)
  └── data/
      └── log_2026-06-23_14-30-00.txt   ← one file per test run
```

## Data Storage

Plain text files in a `data/` subdirectory, one measurement per line, semicolon-separated:

```
timestamp;voltage_mV;current_mA;capacity_mAh;dcir_mOhm;energy_mWh;state
0;4185;0;0;45;0;idle
5;3920;987;1;45;4;discharging
10;3915;985;3;45;7;discharging
```

- New file per test run, named by start time: `log_2026-06-23_14-30-00.txt`
- Header line written at file creation
- Each incoming GET request appends one line
- `data/` directory protected with `.htaccess` (deny direct browsing)

## PHP Endpoints

### `add.php` — Data Receiver

Called by ESP32:
```
GET /add.php?key=<secret>&t=120&v=3742&i=985&cap=328&ri=42&e=1215&state=discharging
```

The first request after an ESP32 boot includes `&new=1` to trigger creation of a new
log file. This ensures each power cycle / reboot starts a fresh file.

Logic:
1. Read GET parameters (t, v, i, cap, ri, e, state)
2. Validate: all numeric except state, state must be in allowed list
3. If no active log file or `new=1`: create new file with header
4. Append semicolon-separated line to current log file
5. If `state=dcir`: also parse `samples` parameter (comma-separated `t_ms:v:i` triples),
   write each sample as a separate line to a DCIR detail file (`dcir_<timestamp>.txt`)
6. Respond with `200 OK` and plain text "OK"

Current log file tracked via a small `data/current.txt` that holds the active filename.

### `data.php` — Data API

Called by the visualization page:
```
GET /data.php                → returns all data from current log
GET /data.php?since=42       → returns only rows after line 42
GET /data.php?file=log_...   → returns data from a specific log file
GET /data.php?list=1         → returns list of available log files
```

Returns JSON:
```json
{
  "header": ["timestamp", "voltage_mV", "current_mA", "capacity_mAh", "dcir_mOhm", "energy_mWh", "state"],
  "rows": [[0, 4185, 0, 0, 45, 0, "idle"], [5, 3920, 987, 1, 45, 4, "discharging"]],
  "totalLines": 245
}
```

### `index.php` — Visualization Page

Single HTML page showing:

1. **Voltage vs. Time** — line chart (discharge overview)
2. **Current vs. Time** — line chart (discharge overview)
3. **Capacity vs. Time** — line chart (mAh accumulated)
4. **Energy vs. Time** — line chart (mWh accumulated)
5. **R_i vs. Time** — line chart (DCIR measurements over the discharge)
6. **Status bar** — current state, last voltage, last current, elapsed time
7. **DCIR result** — displayed as a number (mΩ), updated when a new DCIR value arrives
8. **DCIR detail chart** — voltage and current vs. time during the DCIR sequence (ms resolution), showing the load step and voltage drop
9. **Log file selector** — dropdown to view past test runs

Auto-refreshing: page polls `data.php?since=N` every 10 seconds and appends new points to charts.

Chart.js loaded from CDN.

## Security Considerations

- Validate all GET parameters (type, range) before writing
- Sanitize filenames — never use user input in file paths
- `.htaccess` in `data/` to prevent direct file access
- Shared secret required: ESP32 sends `&key=<secret>` with every request to `add.php`
- `add.php` rejects requests where the key doesn't match, responds with `403 Forbidden`
- Secret stored in `config.h` on ESP32 side and in a separate `secret.php` on server side (both excluded from git)

## ESP32 Side Change

Update `config.h` to point to the web space:

```cpp
#define SERVER_HOST   "www.example.com"
#define SERVER_PORT   80
#define SERVER_PATH   "/accucheck2/add.php"
#define SERVER_KEY    "your_shared_secret"
```

## File Structure

```
accucheck2/
└── webspace/
    ├── add.php                (data receiver endpoint)
    ├── data.php               (data API for visualization)
    ├── index.php              (visualization page with Chart.js)
    ├── secret.php             (shared secret, excluded from git)
    ├── secret.php.example     (template for secret.php)
    └── data/
        └── .htaccess          (deny direct browsing)
```

Deploy by uploading `webspace/` contents to the web space.

## Acceptance Criteria

- [x] `add.php` receives GET request and appends data to log file
- [x] Log file contains correct semicolon-separated data with header
- [x] `data.php` returns valid JSON with correct data
- [x] `data.php?since=N` returns only new rows (incremental update)
- [x] `index.php` shows live-updating charts during discharge test
- [ ] Past test runs viewable via log file selector
- [ ] Invalid/malformed requests are rejected, no file corruption
- [x] Works on standard shared hosting (PHP 7.4+, no special extensions)
