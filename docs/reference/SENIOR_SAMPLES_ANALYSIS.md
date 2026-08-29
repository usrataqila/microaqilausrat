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
