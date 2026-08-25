# Wiring plan — microaqila

Board: **NodeMCU ESP8266** (bag mislabeled "ESP32 DevKit V1"; chip measured
ESP8266EX with esptool, 2026-08-25). Pin names below are the **silkscreen
labels on the board** — match them by label, never by position.

## Pin map (planned — nothing wired yet)

| Signal            | Board pin | GPIO  | Goes to                        | Status    |
|-------------------|-----------|-------|--------------------------------|-----------|
| I2C SDA           | D2        | 4     | OLED `SDA`                     | not wired |
| I2C SCL           | D1        | 5     | OLED `SCL`                     | not wired |
| DHT22 data        | D5        | 14    | DHT module `out`               | not wired |
| Relay 1 (heater)  | D6        | 12    | Relay `IN1`                    | not wired |
| Relay 2 (fan)     | D7        | 13    | Relay `IN2`                    | not wired |
| 3.3 V             | 3V        | —     | OLED `VCC`, DHT `+`            | not wired |
| 5 V (USB)         | VU        | —     | Relay module VCC (TBD after photo check) | not wired |
| GND               | G         | —     | common ground                  | not wired |

**Avoided on purpose:** D3 (GPIO0), D4 (GPIO2), D8 (GPIO15) — boot-strapping
pins that glitch at reset and can prevent boot if a load pulls them wrong.
A relay on these would click at every power-up. D4 also drives the onboard LED.

## Power architecture (decided)

- **ESP + sensors + OLED:** powered from USB (laptop now; any phone charger later).
- **Heater + fan (12 V loads):** powered ONLY from the 12 V LiteOn brick,
  switched through the relay CONTACTS. The 12 V side never touches the ESP.
- Relay coils: 5 V from `VU` (USB 5 V) — to be confirmed against the relay
  module's jumper layout (need close-up photo of its header silkscreen).
- Heater wired through relay **NO (normally open)** contact so that
  de-energized/reset/crashed = heater OFF. Fail-safe by construction.
