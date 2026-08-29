# Analysis of senior samples (for our paper + slides)

Sources (user's Downloads, not copied into repo):
- `Group 4 temp control.pptx` — 12 slides, AIUB capstone presentation
- `MES_project_report temp control.doc` — IEEE-format conference paper

## Their paper structure (this is the format we must match)
Title / authors+affiliation block (AIUB) / Abstract / Keywords /
I. Introduction (A. Background of Study and Motivation, Literature Review) /
II. Methodology / III. System Architecture and Components (A. controller,
B. modules, C. sensor, D. OLED SSD1306, E. fan+heater via relays) /
Results / Conclusion / References [1]-[5].

## Their deck structure (12 slides)
1 Title+course teacher · 2 (blank/agenda) · 3 Objectives + Apparatus ·
4 Methodology · 5 Circuit diagram (simulation + hardware) · 6 System overview ·
7 Real-life applications · 8 Advantages · 9 Limitations · 10 Future improvement ·
11 Conclusion · 12 Thank you.

## THE OPENING FOR US
Their slide 9, limitation #1, verbatim: "Threshold-Based Control The system
uses simple threshold-based logic, which may cause frequent and unnecessary
switching of the fan or heater when the temperature is near the set limits."

That is precisely the problem microaqila's ADAPTIVE threshold solves, and we
can measure it (heater/fan switch counters, fixed vs adaptive mode). So:
- their LIMITATION becomes our CONTRIBUTION
- our Results section = switch-count comparison, which most capstone papers
  lack entirely (they have no quantitative results at all)

## Where we differ from them (must be stated accurately in our paper)
| Them | Us |
|------|-----|
| Arduino UNO + ESP32 module | **ESP8266 (NodeMCU ESP-12F) alone** |
| DHT11 | **DHT22** |
| Mobile app (cloud/Blynk-style) | **ESP hosts its own Wi-Fi AP + web UI, no internet** |
| Fixed thresholds | **Adaptive hysteresis band** |
| Also does room lighting | Not in scope for us |
| No quantitative results | **Measured switch-count comparison** |

## Our real measured data so far (from BUILD_LOG)
- Ambient ~31 C, ~79 %RH (Dhaka, late Aug)
- DHT22 8/8 valid reads at bring-up
- OLED SSD1306 at I2C 0x3C, 128x64
- Heater: 4x 22 ohm 10 W ceramic in parallel = 5.5 ohm, 2.2 A, ~26 W @ 12 V
- Relay contacts 10 A 30 VDC; 3 A fuse; KSD9700 105 C NC thermal cutout
- Measured burst response: +0.4 C in 30 s open air, with post-burst coast
  (thermal lag) - direct evidence of the overshoot that motivates the work

---

# Deep scan of the source files (2026-08-30)

Both files were opened and every embedded image extracted. Findings below are
read directly off their slides/figures, not inferred from prose.

## Their system overview (deck slide 6) — the decisive evidence

Their block diagram is a flowchart:

    DHT11 -> ESP8266 Microcontroller
               |
          Temp < 25 C ?  --Yes-->  Heater ON, Fan OFF
               | No
          Temp > 30 C ?  --Yes-->  Fan ON, Heater OFF
               | No
             Both OFF

**Each branch is a single comparison against one number.** `Temp < 25?` means
the heater switches on at 25.0 and off at 25.0 - the same temperature. There
is NO hysteresis anywhere in their design.

This matters because it is the mechanism behind their own admitted limitation.
We now have both halves from the same source:
  - the CAUSE, drawn in their slide 6 block diagram (single-point thresholds)
  - the EFFECT, admitted in their slide 9 (`frequent and unnecessary switching
    of the fan or heater when the temperature is near the set limits`)

## Their simulation (Proteus, deck slide 5 left)

- Controller: **NodeMCU V3 (ESP8266MOD, AI Thinker)** - not Arduino UNO, not
  ESP32, despite what the report text claims.
- **RL2 (12 V coil)** -> `HEATER 220V`, drawn as a lamp on an AC source.
- **RL1 (5 V coil)** -> `LIGHT 220V`, same. This is the room-lighting feature.
- **FAN** - could not be traced to a relay; appears directly connected.
  [UNCERTAIN - the traces overlap at that point.]
- No opto-isolators or flyback diodes drawn; coils appear driven straight from
  GPIO. Works in simulation only - their real build used a relay module.

## Their hardware (deck slide 5 right)

- **4-channel** 5 V relay module; only 2 channels used per the schematic.
- **Two mains bulbs in proper lamp holders**: one clear incandescent (= the
  HEATER) and one blue coated bulb (= the LIGHT).
- A **12 V DC case fan**, sitting loose on the board.
- Phone running a **"Smart Room"** app (Blynk-style): 29.5 C, 65 %RH,
  **Min Temp 23, Max Temp 32**, and a Room Light toggle.
- NodeMCU on a breadboard.

Note their app exposed min/max thresholds - the same idea as our target/band -
but they are still two single thresholds with no hysteresis between on and off.

## Their report figures

All three images embedded in the .doc are **stock catalogue photos** (Arduino
UNO, ESP32 DevKit, SSD1306 module). **There is not one photograph of their own
build anywhere in the paper.**

## What is safe to claim in our paper

Provable from their own documents, quote-able:
  - their control logic uses single-point thresholds with no hysteresis
    (their slide 6 block diagram)
  - they identified the resulting switching as a limitation (their slide 9)
  - their results section contains no measured numbers

Do NOT claim:
  - that they had no heater (they did - a mains incandescent bulb, visible in
    their hardware photo and labelled in their schematic)
  - anything about the Arduino-UNO-vs-ESP8266 contradiction between their text
    and their diagrams. It is messy, unprovable either way, and not our fight.

## Useful consequence for us

Their block diagram and simulation both say **ESP8266**. We are on the same
chip family, so a fixed-vs-adaptive comparison on our rig is like-for-like and
not confounded by different hardware.
