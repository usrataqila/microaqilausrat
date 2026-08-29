# Wiring plan — microaqila

Board: **NodeMCU ESP8266** (bag mislabeled "ESP32 DevKit V1"; chip measured
ESP8266EX with esptool, 2026-08-25). Pin names below are the **silkscreen
labels on the board** — match them by label, never by position.

## Pin map (status per row — OLED done, rest planned)

| Signal            | Board pin | GPIO  | Goes to                        | Status    |
|-------------------|-----------|-------|--------------------------------|-----------|
| I2C SDA           | D2        | 4     | OLED `SDA`                     | **WIRED [M]** |
| I2C SCL           | D1        | 5     | OLED `SCL`                     | **WIRED [M]** |
| DHT22 data        | D5        | 14    | DHT module `out`               | **WIRED [M]** |
| Relay 1 (heater)  | D6        | 12    | Relay J2 `IN1`                 | **WIRED [M]** |
| Relay 2 (fan)     | D7        | 13    | Relay J2 `IN2`                 | **WIRED [M]** |
| 3.3 V             | 3V        | —     | OLED `VCC`, DHT `+`            | **WIRED [M]** |
| 5 V (USB)         | VU        | —     | Relay J1 `RY-VCC` (coil supply) | **WIRED [M]** |
| GND               | G         | —     | common ground (OLED `GND`, DHT `-`) | **WIRED [M]** |

**Avoided on purpose:** D3 (GPIO0), D4 (GPIO2), D8 (GPIO15) — boot-strapping
pins that glitch at reset and can prevent boot if a load pulls them wrong.
A relay on these would click at every power-up. D4 also drives the onboard LED.

## Power architecture (decided)

- **ESP + sensors + OLED:** powered from USB (laptop now; any phone charger later).
- **Heater + fan (12 V loads):** powered ONLY from the 12 V LiteOn brick,
  switched through the relay CONTACTS. The 12 V side never touches the ESP.
- Relay coils: 5 V from `VU` via J1 `RY-VCC`, **jumper removed** (split
  supply). Opto input side runs at 3.3 V from `3V` via J2 `VCC`, so the
  ESP8266's 3.3 V logic matches the opto reference exactly. Confirmed against
  close-up photos of J1/J2 silkscreen, 2026-08-25.
- Heater wired through relay **NO (normally open)** contact so that
  de-energized/reset/crashed = heater OFF. Fail-safe by construction.

## Verified subsystems

### OLED — DONE [M] 2026-08-25
Wired `GND`→`G`, `VCC`→`3V`, `SCL`→`D1`, `SDA`→`D2`. Bus scan reports **0x3C**;
`firmware/diag_v2` inits SSD1306 at **128x64** and renders — user visually
confirmed the checkerboard test screen. Address ACKs every frame, heap flat.
Powered from `3V`, never `VU`: the ESP8266's I/O is 3.3 V logic.

### DHT22 — DONE [M] 2026-08-25
Module header is silkscreened `+  out  -` on **both** faces (front reads
`+ out -` left→right, back is mirrored `- out +`) — match by the printed label,
not by position or wire colour. On-board pull-up R1 is fitted, so no external
resistor is needed. DHT22 runs happily at 3.3 V (datasheet range 3.3-6 V).

| DHT22 pin | NodeMCU pin |
|-----------|-------------|
| `+`       | `3V`  (the spare one at the far end of the right header) |
| `out`     | `D5`  (GPIO14) |
| `-`       | `G`   (the spare one next to `TX`) |

Verified with `firmware/diag_v3`: **8/8 reads valid, zero failures**, 31.0 C /
79 %RH, heap flat. Sensing + display half of the project is complete.

### Relay module control side — WIRED [M] 2026-08-26, both relays click
Headers read off close-up photos (2026-08-25):
`J1 = RY-VCC | VCC | GND` (jumper shipped on RY-VCC+VCC), `J2 = GND | IN1 | IN2 | VCC`.

Step 1: pull the J1 jumper cap off, park it hanging on the single `GND` pin.
Then six wires (NodeMCU left header has free `VU`/`3V`/`G` pins; the right
header's 3V/G pairs are taken by OLED + DHT):

| Relay module pin | NodeMCU pin | Why |
|------------------|-------------|-----|
| J1 `RY-VCC`      | `VU`        | 5 V coil supply straight from USB |
| J1 `GND`         | `G`         | coil return |
| J2 `VCC`         | `3V`        | opto input side at 3.3 V |
| J2 `GND`         | `G`         | input-side reference |
| J2 `IN1`         | `D6`        | K1 (heater channel, wired LAST) |
| J2 `IN2`         | `D7`        | K2 (fan channel) |

NOTHING goes on the K1/K2 screw terminals in this step — no 12 V, no loads.
`firmware/diag_v4` (already flashed) holds both relays OFF for 8 s at boot,
then clicks K1 for 1.2 s and K2 for 1.2 s every ~8 s cycle.
[G] LOW-trigger assumed — the click pattern verifies; NO/COM/NC will be
identified by multimeter continuity, never by guessing.

### K2 screw terminals — IDENTIFIED [M] 2026-08-27
ESP-as-continuity-tester (diag_v6, D4 source / D0 sense): with both screws
properly clamped, the pair MIDDLE + EDGE-OUTER (board-corner side) of the K2
trio conducts ONLY when K2 is energized => COM+NO pair. Fan circuit uses
exactly these two screws. The third screw (inner outer) = NC, never used.
K1's trio gets its own probe test before the heater goes on.
