# Phase 4b — DCIR Measurement Tooling

## Goal

Provide a Python tool that automates repeated DCIR measurements via serial,
generates interactive HTML reports with Plotly, and stores them in the repository
for documentation purposes.

## Tool

`tools/dcir_plot.py` — connects to the ESP32 via COM port, runs multiple DCIR
measurements, and creates an HTML report with interactive voltage/current charts.

### Usage

```
pip install pyserial plotly
python tools/dcir_plot.py                     # auto-detect port, 5 measurements
python tools/dcir_plot.py --port COM10        # specify port
python tools/dcir_plot.py --count 10          # more measurements
python tools/dcir_plot.py --pause 10          # longer pause between runs
python tools/dcir_plot.py --output path.html  # custom output path
```

### Features

- Auto-detects ESP32 COM port (CP210x, CH340, FTDI)
- Runs N DCIR measurements with configurable pause
- Parses float-precision voltage/current samples from serial
- Generates interactive Plotly HTML report with:
  - Voltage vs. time chart per run
  - Current vs. time chart per run
  - Summary with R_i values, mean, and spread
- Opens report in browser automatically

## Reports

HTML reports are stored in `doc/reports/` and committed to the repository
to document measurement results and cell characterization progress.

## Acceptance Criteria

- [x] Script auto-detects COM port
- [x] DCIR measurements run successfully (5 runs, 60 samples each)
- [x] HTML report generated with interactive Plotly charts
- [x] Float precision gives ~2 mOhm resolution (spread < 3 mOhm)
- [x] Representative report committed to `doc/reports/`
