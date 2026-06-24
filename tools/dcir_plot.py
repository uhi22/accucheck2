"""
DCIR measurement tool for accucheck2.
Finds the ESP32 on a COM port, runs repeated DCIR measurements,
and creates an HTML report with interactive Plotly charts.

Usage: python dcir_plot.py [--port COMx] [--count N] [--pause SECONDS]

Requirements: pip install pyserial plotly
"""

import argparse
import os
import time
import webbrowser
from datetime import datetime

import serial
import serial.tools.list_ports
import plotly.graph_objects as go
from plotly.subplots import make_subplots


def find_esp32():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = (p.description or "").lower()
        vid = p.vid or 0
        if vid in (0x10C4, 0x1A86, 0x0403) or "cp210" in desc or "ch340" in desc or "usb" in desc:
            print(f"Found: {p.device} - {p.description}")
            return p.device
    for p in ports:
        try:
            with serial.Serial(p.device, 115200, timeout=2) as s:
                s.write(b"status\n")
                time.sleep(0.5)
                resp = s.read(s.in_waiting).decode(errors="replace")
                if "State:" in resp or "READY" in resp:
                    print(f"Found accucheck2 on {p.device}")
                    return p.device
        except (serial.SerialException, OSError):
            pass
    return None


def run_dcir(ser):
    ser.reset_input_buffer()
    ser.write(b"dcir\n")

    lines = []
    start = time.time()
    while time.time() - start < 10:
        if ser.in_waiting:
            line = ser.readline().decode(errors="replace").strip()
            if line:
                lines.append(line)
                if "failed" in line.lower():
                    return None
                if len(lines) > 5 and not line[0].isdigit():
                    break
        else:
            time.sleep(0.01)

    time.sleep(0.2)
    while ser.in_waiting:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            lines.append(line)

    result = {"ri_mOhm": None, "v_rest": None, "i_rest": None,
              "v_load": None, "i_load": None, "samples": []}

    in_samples = False
    for line in lines:
        if line.startswith("R_i = "):
            result["ri_mOhm"] = float(line.split("=")[1].strip().split()[0])
        elif line.startswith("V_rest="):
            parts = line.replace(",", "").split()
            for p in parts:
                if p.startswith("V_rest="): result["v_rest"] = float(p.split("=")[1])
                if p.startswith("I_rest="): result["i_rest"] = float(p.split("=")[1])
                if p.startswith("V_load="): result["v_load"] = float(p.split("=")[1])
                if p.startswith("I_load="): result["i_load"] = float(p.split("=")[1])
        elif "Samples" in line:
            in_samples = True
        elif in_samples:
            parts = line.split("\t")
            if len(parts) == 3:
                try:
                    t_ms = int(parts[0])
                    v_mv = float(parts[1])
                    i_ma = float(parts[2])
                    result["samples"].append((t_ms, v_mv, i_ma))
                except ValueError:
                    pass

    if result["ri_mOhm"] is None:
        return None
    return result


def create_report(results, output_path):
    n = len(results)
    ri_values = [r["ri_mOhm"] for r in results]
    ri_mean = sum(ri_values) / len(ri_values)
    ri_spread = max(ri_values) - min(ri_values)

    fig = make_subplots(
        rows=n, cols=2,
        subplot_titles=[
            title
            for idx, r in enumerate(results)
            for title in (
                f"Run {idx+1}: Voltage (R_i = {r['ri_mOhm']:.1f} mOhm)",
                f"Run {idx+1}: Current (V_rest={r['v_rest']:.2f} V_load={r['v_load']:.2f})"
            )
        ],
        vertical_spacing=0.08,
        horizontal_spacing=0.08,
    )

    for idx, r in enumerate(results):
        samples = r["samples"]
        if not samples:
            continue
        t = [s[0] for s in samples]
        v = [s[1] for s in samples]
        i = [s[2] for s in samples]
        row = idx + 1

        fig.add_trace(
            go.Scatter(x=t, y=v, mode="lines+markers", name=f"V run {row}",
                       marker=dict(size=4), line=dict(color="royalblue")),
            row=row, col=1
        )
        fig.update_yaxes(title_text="Voltage (mV)", row=row, col=1)
        fig.update_xaxes(title_text="Time (ms)", row=row, col=1)

        fig.add_trace(
            go.Scatter(x=t, y=i, mode="lines+markers", name=f"I run {row}",
                       marker=dict(size=4), line=dict(color="crimson")),
            row=row, col=2
        )
        fig.update_yaxes(title_text="Current (mA)", row=row, col=2)
        fig.update_xaxes(title_text="Time (ms)", row=row, col=2)

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    fig.update_layout(
        title_text=(
            f"accucheck2 DCIR Report - {timestamp}<br>"
            f"<sup>R_i values: {['%.1f' % v for v in ri_values]} mOhm | "
            f"Mean: {ri_mean:.1f} mOhm | Spread: {ri_spread:.1f} mOhm</sup>"
        ),
        height=350 * n,
        showlegend=False,
    )

    fig.write_html(output_path)
    print(f"Report saved to: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="accucheck2 DCIR measurement & plot")
    parser.add_argument("--port", help="COM port (auto-detect if omitted)")
    parser.add_argument("--count", type=int, default=5, help="Number of DCIR measurements (default: 5)")
    parser.add_argument("--pause", type=float, default=5.0, help="Pause between measurements in seconds (default: 5)")
    parser.add_argument("--output", help="Output HTML file path (default: auto-generated in tools/)")
    args = parser.parse_args()

    port = args.port or find_esp32()
    if not port:
        print("ERROR: Could not find ESP32. Use --port COMx to specify manually.")
        return

    print(f"Connecting to {port}...")
    ser = serial.Serial(port, 115200, timeout=2)
    time.sleep(2)
    ser.reset_input_buffer()

    ser.write(b"status\n")
    time.sleep(0.5)
    resp = ser.read(ser.in_waiting).decode(errors="replace")
    if "State:" not in resp:
        print(f"WARNING: Unexpected response from device:\n{resp}")

    results = []
    for i in range(args.count):
        print(f"\nDCIR measurement {i+1}/{args.count}...")
        r = run_dcir(ser)
        if r:
            print(f"  R_i = {r['ri_mOhm']:.1f} mOhm  ({len(r['samples'])} samples)")
            results.append(r)
        else:
            print("  Measurement failed.")
        if i < args.count - 1:
            print(f"  Waiting {args.pause}s...")
            time.sleep(args.pause)

    ser.close()

    if not results:
        print("\nNo successful measurements.")
        return

    ri_values = [r["ri_mOhm"] for r in results]
    ri_strs = [f"{v:.1f}" for v in ri_values]
    print(f"\nResults: [{', '.join(ri_strs)}] mOhm")
    print(f"Mean: {sum(ri_values)/len(ri_values):.1f} mOhm, Spread: {max(ri_values)-min(ri_values):.1f} mOhm")

    if args.output:
        output_path = args.output
    else:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_path = os.path.join(os.path.dirname(__file__), f"dcir_report_{timestamp}.html")

    create_report(results, output_path)
    webbrowser.open(f"file://{os.path.abspath(output_path)}")


if __name__ == "__main__":
    main()
