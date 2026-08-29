"""Reset the board and capture its serial output for N seconds.
Usage: python serial_read.py [COM6] [seconds] [baud]"""
import sys, time, serial

# Windows consoles default to cp1252; ESP boot ROM emits non-UTF8 garbage at reset.
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 8
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

s = serial.Serial()
s.port, s.baudrate, s.timeout = port, baud, 0.5
s.dtr = False; s.rts = False
s.open()
# pulse RTS with DTR deasserted -> clean reset into normal run mode
s.rts = True; time.sleep(0.1); s.rts = False
end = time.time() + secs
buf = b""
while time.time() < end:
    buf += s.read(4096)
s.close()
print(buf.decode("utf-8", "replace"))
