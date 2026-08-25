# Build log — microaqila

Project: Adaptive threshold temperature control (EEE4103 capstone, G7 Sec O).
Convention: **[M]** = measured fact, **[G]** = guess/assumption to verify.

## 2026-08-25 — session 1: board bring-up

- [M] Board enumerates as **USB-SERIAL CH340 on COM6**, driver OK (Windows 11
  already had it — no driver install needed).
- [M] `esptool chip_id` → **ESP8266EX**, ESP-12F module, MAC bc:dd:c2:6b:e2:63,
  crystal 26 MHz. The bag says "ESP32 DevKit V1" — **label is wrong**, this is
  a NodeMCU ESP8266. Proposal says ESP32 → decision needed (see report).
- [M] Photos: DHT22 (proposal says DHT11 — we have the better sensor),
  SSD1306-style 0.96" OLED (back silkscreen JMD0.96D-1, addr select 0x78/0x7A
  → expect I2C 0x3C), 2-ch 5V relay SRD-05VDC-SL-C (10A 250VAC / 10A 30VDC
  contacts), 12V 5–7A LiteOn PA-1061-0 brick, ADDA 12V 0.25A 80mm fan,
  handmade nichrome-on-mica heater (resistance UNKNOWN — must measure),
  MB-102 breadboard, jumpers, 2× DC barrel screw-terminal adapters (1 male,
  1 female), Sanwa CD800a multimeter.
- Toolchain installed (local, per project rules): arduino-cli 1.5.2-rc.1 in
  `tools/arduino-cli/`, esptool 5.3.1 + pyserial via pip --user (Python 3.13).
  Board cores/libs land in `%LOCALAPPDATA%\Arduino15` (user profile).
- Wrote `firmware/diag_v1` (chip health + I2C scanner, no wiring needed).
- [M] ESP8266 core 3.1.2 + libs installed (DHT 1.4.7, Unified Sensor 1.1.15,
  GFX 1.12.6, SSD1306 2.5.17, BusIO 1.17.4). NOTE: `Documents\` is blocked for
  CLI writes (Controlled Folder Access) — sketchbook relocated to
  `C:\Users\AUsrat\.arduino-user`.
- [M] diag_v1 compiled (22% flash), uploaded to COM6, hash verified. Serial
  readback: Chip ID 0x006BE263 (matches esptool), 4096 KB flash, 80 MHz,
  core 3.1.2, heap 51424 stable, I2C scan runs, no devices found (correct —
  nothing wired). Board bring-up COMPLETE.
- Next: measure heater cold resistance (defines power draw @12V vs 5A brick).
