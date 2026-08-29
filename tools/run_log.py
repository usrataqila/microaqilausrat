"""Capture a timed control_v4 run to a log + CSV, for the report's Results.

Usage:  python run_log.py [minutes] [port] [--noreset]
        python run_log.py 20                 -> 20 min run on COM6
        python run_log.py 20 COM6 --noreset  -> do not reset the board first

Before starting: open http://192.168.4.1 and press ADAPTIVE or FIXED. The mode
is stored in EEPROM, so it survives the reset this script performs, and the
reset gives every run the same clean starting point (counters at zero, band
back at the saved base).

Writes into data/:
  run_<MODE>_<mins>min_<stamp>.log   raw serial, exactly as the board sent it
  run_<MODE>_<mins>min_<stamp>.csv   one row per data line, for a graph
and prints the summary line that goes straight into the report table.
"""
import csv
import os
import re
import sys
import time
from datetime import datetime

import serial

# Windows consoles default to cp1252; ESP boot ROM emits non-UTF8 garbage.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

args = [a for a in sys.argv[1:] if not a.startswith("--")]
flags = {a for a in sys.argv[1:] if a.startswith("--")}

mins = float(args[0]) if len(args) > 0 else 20.0
port = args[1] if len(args) > 1 else "COM6"
baud = int(args[2]) if len(args) > 2 else 115200
do_reset = "--noreset" not in flags

# [   3m] t=31.0 h=80.1 heat=ON  fan=off band=0.50 ADAPT sw=1/0 tot=1
DATA = re.compile(
    r"\[\s*(\d+)m\]\s+t=(-?[\d.]+)\s+h=(-?[\d.]+)\s+"
    r"heat=(\w+)\s+fan=(\w+)\s+band=([\d.]+)\s+(\w+)\s+"
    r"sw=(\d+)/(\d+)\s+tot=(\d+)"
)
# ~~~ ADAPT: 5 switches last window -> band 0.50 -> 0.60
ADAPT = re.compile(
    r"~~~ ADAPT: (\d+) switches last window -> band ([\d.]+) -> ([\d.]+)"
)

here = os.path.dirname(os.path.abspath(__file__))
outdir = os.path.join(os.path.dirname(here), "data")
os.makedirs(outdir, exist_ok=True)

print(f"Opening {port} at {baud}, capturing {mins:g} min "
      f"({'with' if do_reset else 'without'} a reset)...")

s = serial.Serial()
s.port, s.baudrate, s.timeout = port, baud, 0.5
s.dtr = False
s.rts = False
s.open()
if do_reset:
    # pulse RTS with DTR deasserted -> clean reset into normal run mode
    s.rts = True
    time.sleep(0.1)
    s.rts = False

stamp = datetime.now().strftime("%Y%m%d-%H%M")
end = time.time() + mins * 60
raw, rows, adapts = [], [], []
mode = "UNKNOWN"
pending = b""
next_note = time.time() + 60

try:
    while time.time() < end:
        pending += s.read(4096)
        while b"\n" in pending:
            line, pending = pending.split(b"\n", 1)
            text = line.decode("utf-8", "replace").rstrip("\r")
            raw.append(text)
            m = DATA.search(text)
            if m:
                mode = m.group(7)
                rows.append({
                    "minute": int(m.group(1)),
                    "temp_c": float(m.group(2)),
                    "humidity_pct": float(m.group(3)),
                    "heater": 1 if m.group(4) == "ON" else 0,
                    "fan": 1 if m.group(5) == "ON" else 0,
                    "band_c": float(m.group(6)),
                    "mode": m.group(7),
                    "heater_switches": int(m.group(8)),
                    "fan_switches": int(m.group(9)),
                    "total_switches": int(m.group(10)),
                })
            a = ADAPT.search(text)
            if a:
                adapts.append((int(a.group(1)), float(a.group(2)), float(a.group(3))))
                print(f"  band moved {a.group(2)} -> {a.group(3)} "
                      f"({a.group(1)} switches that window)")
        if time.time() >= next_note:
            next_note += 60
            left = (end - time.time()) / 60
            last = rows[-1] if rows else None
            if last:
                print(f"  {left:4.0f} min left | {last['temp_c']:.1f} C | "
                      f"band {last['band_c']:.2f} | "
                      f"{last['total_switches']} switches so far")
except KeyboardInterrupt:
    print("\nStopped early by user - saving what was captured.")
finally:
    s.close()

base = os.path.join(outdir, f"run_{mode}_{mins:g}min_{stamp}")
with open(base + ".log", "w", encoding="utf-8") as f:
    f.write("\n".join(raw) + "\n")
if rows:
    with open(base + ".csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

print("\n" + "=" * 60)
if rows:
    first, last = rows[0], rows[-1]
    print(f"  MODE            {mode}")
    print(f"  DURATION        {last['minute']} min")
    print(f"  TOTAL SWITCHES  {last['total_switches']}  "
          f"(heater {last['heater_switches']}, fan {last['fan_switches']})")
    print(f"  BAND            started {first['band_c']:.2f} C, "
          f"ended {last['band_c']:.2f} C  ({len(adapts)} adjustments)")
    temps = [r["temp_c"] for r in rows]
    print(f"  TEMPERATURE     min {min(temps):.1f} C, max {max(temps):.1f} C")
    print(f"  SAMPLES         {len(rows)}")
else:
    print("  NO DATA LINES CAPTURED - is control_v4 actually running?")
print("=" * 60)
print(f"\nSaved:\n  {base}.log\n  {base}.csv")
