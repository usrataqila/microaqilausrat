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

## 2026-08-25 — session 2: OLED wiring

- [M] Re-verified board on COM6 before touching anything: serial readback gives
  **Chip ID 0x006BE263** — byte-for-byte the same ESP8266 as session 1. The
  board now plugged in is the **NodeMCU ESP8266**, not an ESP32 (user called it
  "the esp32"; the bag label is still wrong, the silicon has not changed).
- [M] diag_v1 still resident and running: core 3.1.2, 4096 KB flash, 80 MHz,
  heap 51424 stable, reset reason "External System", I2C scan reports **no
  devices** → confirms nothing is wired to D1/D2 yet.
- Fixed `tools/serial_read.py`: forced stdout to UTF-8 with `errors="replace"`
  (Windows cp1252 console crashed on the ESP boot-ROM garbage at 74880 baud).
- [M] OLED silkscreen (front, resources/oled2.jpeg), left→right: `GND VCC SCL
  SDA`. Back (oled.jpeg): JMD0.96D-1, address-select pads `0x7B`/`0x7A`
  (8-bit) → 7-bit 0x3C or 0x3D. Do NOT assume 0x3C — let the scanner report it.
- [M] NodeMCU silkscreen (resources/esp2.jpeg), right header top→bottom:
  D0 D1 D2 D3 D4 3V G D5 D6 D7 D8 RX TX G 3V. Left header: A0 G VU S3 S2 S1
  SC SO SK G 3V EN RST G VIN. All four OLED wires land on the right header.
- Next: user wires OLED (4 jumpers), then serial readback should show the I2C
  address appear.
- [M] OLED WIRED AND ANSWERING: diag_v1 scan reports **0x3C** on every pass,
  heap flat at 51424 → all four jumpers good (GND-G, VCC-3V, SCL-D1, SDA-D2).
- Wrote + flashed `firmware/diag_v2` (OLED render test: probes 0x3C, inits
  SSD1306, cycles 3 screens — text / all-pixels-on / checkerboard+border —
  and re-probes the address every frame so a dropout shows as data).
- [M] diag_v2 readback: `Probe 0x3C: ACK`, `SSD1306 init: OK (128x64)`,
  heap 51432 → 50400 after the 1 KB framebuffer alloc, then **flat across
  frames (no leak)**, address still ACK on every frame. Panel size 128x64
  accepted by init.
- NOTE: SSD1306 over I2C is write-only in practice — firmware cannot read back
  what the glass shows. "Pixels actually lit" is a VISUAL check by the user,
  everything up to the panel is confirmed electrically.
- [M] **USER VISUAL CONFIRM: OLED renders** — checkerboard test screen seen on
  the glass. OLED subsystem is DONE (bus + driver + panel all proven).
- [M] DHT22 module silkscreen (resources/temphumid*.jpeg): 3-pin header marked
  `+ out -`, printed on both faces (back mirrored). Pull-up R1 fitted on-module.
- Next: user wires DHT22 (`+`→3V, `out`→D5, `-`→G), then flash a sensor read
  that puts live temp/humidity on the OLED.
- [M] **DHT22 WORKS.** Wrote + flashed `firmware/diag_v3` (DHT read + live OLED
  readout + running pass/fail tally). Readback: `SSD1306 init: OK`, then
  **8/8 valid reads, zero failures**, t = 30.9-31.0 C, h = 79.0-79.2 %RH,
  heap flat at 50112 (no leak). Values are physically sensible for Dhaka in
  late August, and temp/humidity track independently → not a stuck bus.
- Sense + display half of the capstone is now COMPLETE: the board is a working
  thermometer showing live C and %RH on the OLED.
- [M] Ambient baseline for later control tests: **~31 C, ~79 %RH**. Any heater
  setpoint must sit above this; cooling below ambient is not possible with a fan.
- BLOCKER for the 12 V side: handmade heater cold resistance still UNMEASURED.
  It sets the current draw against the 12 V 5-7 A brick and must be known before
  any relay contact is wired.
- Heater DEFERRED to last by user's call (handmade, riskiest part). Plan changed:
  prove the whole switching chain with the **fan** first — it is a factory part,
  12 V 0.25 A = 3 W, against contacts rated 10 A 30 VDC, so ~40x margin.
- [M] Relay module, read off resources/relay.jpeg (enhanced crops):
  - Relays are **SRD-05VDC-SL-C** → **5 V coils**, so coil power must come from
    `VU` (USB 5 V), never from `3V`.
  - Contacts printed 10 A 250 VAC / 10 A 30 VDC / 10 A 125 VAC / 10 A 28 VDC.
  - Screw terminals are silkscreened **only `K1` and `K2`** — there are NO
    NO/COM/NC markings on either face. Those will be identified by CONTINUITY
    MEASUREMENT (de-energized: COM-NC closed, COM-NO open), not by guessing.
  - Two bottom headers: **J1** (3-pin, jumper cap fitted) and **J2** (4-pin).
    Partial silkscreen legible: `RY-` (likely RY-VCC) and `ND` (likely GND).
  - BLOCKED: the J1/J2 pin labels are cut off at the board edge and shot through
    the bag, so the 4-pin order (GND/IN1/IN2/VCC?) is NOT readable. Getting VCC
    and GND backwards would destroy the module → asked user for a close-up.
- [M] Relay headers read off new close-up photos (bag off):
  - **J1 (3-pin): `RY-VCC | VCC | GND`**, jumper cap shipped bridging
    RY-VCC↔VCC (single-supply mode).
  - **J2 (4-pin): `GND | IN1 | IN2 | VCC`**.
  - Optos are PC817 → classic opto-isolated, LOW-trigger topology [G until
    click test]. Board name silkscreen: "2 Relay Module".
- DECISION — split-supply wiring (the reason that J1 jumper exists):
  ESP8266 GPIO is 3.3 V but the module is a 5 V part. With the jumper in and
  VCC=5 V, a 3.3 V "HIGH" leaves ~1.7 V across the opto LED path → LED can
  half-conduct and the relay may not release cleanly. Fix: REMOVE the jumper,
  feed J2 `VCC` (opto input side) from 3V, and J1 `RY-VCC` (coils) from VU
  (USB 5 V). GPIO swings then match the opto reference exactly, and coil
  current never loads the 3.3 V regulator.
- Coil budget: 2x SRD-05 coils ~70 mA each = ~140 mA worst case on VU → fine
  for USB. Boot check: D6/D7 float during boot; floating opto input = no LED
  current = relay OFF, so the module cannot pulse at reset (unlike D3/D4/D8).
- [M] Wrote + flashed `firmware/diag_v4` (relay click test, control side only).
  Readback with nothing wired: 8 s settle runs, then K1/K2 cycle with pin
  levels printed, DHT still ok (31.0 C / 79.5 %), heap flat 50072. OLED shows
  live K1/K2 state. Ready for user to wire the relay control side.

## 2026-08-26 — session 3: relay control side

- [M] User wired J1/J2 per plan (jumper removed, split supply: RY-VCC<-VU,
  opto VCC<-3V). USER HEARS BOTH RELAYS CLICK (two distinct clicks/cycle).
  Serial readback in the same window: K1/K2 cycling as commanded, DHT still
  ok (30.7 C / 80.6 %RH), heap flat 50072-50296, no resets while coils
  switch -> no brownout on VU. Control chain proven for both channels.
- OPEN [G]: trigger polarity not yet pinned. Firmware assumes LOW-trigger.
  If the module were HIGH-trigger the user would hear the same two clicks
  per cycle, just inverted (energized during "off" windows + a click at
  plug-in). Deciding observation requested: during the 8 s settle window
  right after plug-in, are the relay-board LEDs DARK (=LOW-trigger, as
  assumed) or LIT (=HIGH-trigger, flip RELAY_ACTIVE_LOW to 0)?
  This must be nailed before any heater wiring - it IS the fail-safe.
- [M] **Trigger polarity CONFIRMED LOW-trigger** by user observation: after
  re-plug, relay-board LEDs dark through the whole 8 s settle window, no click
  at plug-in. RELAY_ACTIVE_LOW=1 is correct. Fail-safe chain now proven
  end-to-end: boot/reset/crash = coils de-energized = (future) heater OFF.
- User declines multimeter (doesn't trust it) -> switching to EMPIRICAL
  terminal ID: use the fan itself as the indicator, wired in series through
  K2 while diag_v5 alternates 60 s OFF / 60 s ON. Safe by construction: the
  fan (12V 0.25A) is always in the loop, so no combination of screw choices
  can overcurrent anything; worst outcomes are "spins at wrong time" or
  "doesn't spin". The ONLY forbidden config (stated to user in bold): brick +
  and - must never meet without the fan between them (dead short).
- [M] Fan label: ADDA AD0812HS-A70GL, DC 12V 0.25A, two wires (red/black).
- [G] LiteOn brick assumed center-positive (standard); if wrong, brushless fan
  simply won't spin - revisit if "never spins" is reported.
- Config 1 issued: brick+ -> K2 MIDDLE screw; K2 LEFT screw -> fan RED;
  fan BLACK -> brick-. Expect: spins only during "K2 ON" if L,M = the
  energized-closed pair.
- [M] SECOND BAG SURPRISE: user reports brick plug and BOTH barrel adapters
  are male (all "tube with hole" ends) - nothing mates. The kit is missing
  the female jack-to-screw adapter. SHOPPING: "DC female jack to screw
  terminal adapter" - take the brick plug to the shop to match by fit.
- Pivot: fan test needs 12 V, so terminal ID is now done by the ESP ITSELF
  as continuity tester (user distrusts multimeter, no 12 V needed):
  diag_v6 drives 3.3 V out of D4 (GPIO2) and listens on D0 (GPIO16,
  INPUT_PULLDOWN_16); pins chosen so no boot-strap can be violated in any
  contact state. K2 alternates 6 s off/on, firmware prints+displays whether
  the wired screw pair conducts in each state.
- [M] diag_v6 flashed; with screws unwired reports open/open ("no contact in
  this pair") on every cycle -> no false positives. Awaiting user's 2 probe
  wires on K2 M+L screws.
- Shopping list issued for 2026-08-27: (1) DC FEMALE jack -> screw-terminal
  adapter x2, matched BY FIT against the brick plug in the shop; (2) roll of
  electrical tape; optional: ACS712 5A current-sensor module (lets the ESP
  measure heater current itself, sidesteps the distrusted multimeter);
  optional pending decision: real ESP32 DevKit V1 vs amending report to
  ESP8266.
- Cross-checked shopping list against the one the user got from claude.ai
  (which recommended retiring the handmade heater -> AGREED, PTC pad instead).
  Final merged list issued; corrections vs that list: no new multimeter (Sanwa
  CD800a owned, to be validated against ESP 3.3V/5V rails), 1N4007s optional
  (module has onboard flyback D1/D2), wire bumped 22AWG -> 1mm2 (18-20AWG),
  thermal fuses conditional (mandatory only if fallback = ceramic resistors).
  Fallback heater if no PTC in shop: 4x 22 ohm 10W ceramic resistors in
  parallel (5.5 ohm, ~26W @ 12V, 2.2A) + thermal fuses.
- HEATER DECISION [pending final purchase]: handmade nichrome RETIRED from
  the power path; PTC pad (or resistor bank) replaces it. Handmade element
  goes in the report as "replaced after risk assessment".
- User decision: buying a NEW multimeter regardless (won't use the Sanwa).
  Fine - a fresh meter also lets us cross-check the Sanwa later. Meter-based
  measurements are back on the menu once it arrives.
- List trimmed on user request: SKIP choc block (two-wires-per-screw on the
  jack/relay terminals instead), SKIP ACS712 (new multimeter covers current),
  SKIP ESP32 (KEEP ESP8266 on breadboard - report gets amended, this is now
  the decided platform). Stranded wire kept but halved to 1m red + 1m black
  (heater path only; fan stays on dupont). Chamber = any box over sensor+
  heater at demo time only, not a purchase. Build base = user's exam writing
  board.

## 2026-08-27 — session 4: shopping haul verified (9 new photos in resources/)

- [M] NO PTC pad in shop -> fallback heater it is: 4x ceramic "10W22RJ"
  (10 W, 22 ohm, 5%). Parallel bank = 5.5 ohm -> 12 V gives 2.18 A / 26 W
  total, 6.5 W per resistor (65% of rating). Fits brick (5-7 A), relay
  (10 A), and a 3 A fuse (with fan: total ~2.4 A).
- [M] Thermal protection: 2x KSD9700 250V 5A 105 C - these are auto-RESET
  thermal SWITCHES (not one-shot fuses). NC/NO polarity unknown -> must be
  verified (meter or ESP probe) before trusting; goes in series with heater.
  Placement decided later: sense runaway without nuisance-tripping (resistor
  surface at 6.5 W runs well above 105 C in free air; mount on bank plate
  edge, not the hottest face).
- [M] Multimeter: UNI-T UT33D+ (beeper, 10A jack, NCV) - proper instrument.
- [M] Jacks: 2x male + 2x FEMALE with +/- screw blocks. Click-fit vs brick
  to be confirmed at home.
- [M] Inline glass-fuse holder + 4 glass fuses; cap engravings unreadable in
  photo - user to read which are 3A vs 5A. 3A is the one we use.
- [M] Red+black hookup wire + electrical tape. Gauge unverifiable from photo;
  judge during heater assembly (2.2 A continuous).
- Pending step UNCHANGED: D4 -> K2 middle screw, D0 -> K2 left screw
  (diag_v6 still flashed and waiting).
- [M] USER CONFIRMS: both female jacks CLICK-FIT the brick plug. 12 V path
  hardware is complete.
- [M] Fuse caps read by user: "F3A..." and "F5A..." pairs, kept separated.
  3 A pair = the one for our 12 V line (heater bank 2.2 A + fan 0.25 A).
- [M] Probe round 1: open/open on the wired pair AND user hears the click on
  the LEFT relay (viewing from the screw side) -> user had wired the other
  group's screws: my "K2 = right group" instruction was given in the LABEL-
  side orientation, user works from the SCREW side (mirrored). Lesson for all
  future screw instructions: name screws by "the clicking relay's own trio"
  (physical adjacency + its indicator LED), never by left/right.
- [M] Photos of the wired board: all 6 control wires visually confirmed
  correct (J1 middle VCC pin properly empty). ROOT CAUSE candidate for the
  open/open reads found: user has NO screwdriver that fits the terminal
  screws -> screws never tightened -> probe pins resting unclamped in the
  cage = no contact pressure. Fix: improvised driver (scissors tip / knife /
  nail-file / glasses-kit), counter-clockwise to open, pin fully in,
  clockwise snug, tug test. Noted: small flathead screwdriver is REQUIRED
  equipment for the rest of the build (jacks, fan, heater all screw-clamp).
- [M] *** K2 SCREW TERMINALS IDENTIFIED *** After tightening both screws:
  probe pair reads open when de-energized, CLOSED when energized, on 2/2
  cycles -> the pair = COM+NO. In the user's screw-side view of the K2 trio:
  MIDDLE screw + EDGE-outer screw (board-corner side) = the fan pair
  (conducts only when relay ON). Remaining screw (inner outer, nearest K1
  trio) = NC, never to be used. Root cause of earlier open/open confirmed:
  unclamped screws. Which of middle/edge is COM vs NO individually is
  irrelevant for a single load (contacts are symmetric).
- K1 trio: NOT assumed to mirror K2 - will be probed the same way before the
  heater is wired.
- [M] *** FIRST 12V SUCCESS *** User wired jack->3A fuse->K2(COM/NO)->fan
  loop; fan spins with every relay click, stops when released. Full chain
  firmware->GPIO->opto->coil->contacts->fuse->12V->fan PROVEN. Brick polarity
  matches jack markings (fan spins => center-positive confirmed [M]).
- Next: control_v1 - first closed-loop firmware (fan-only cooling thermostat
  with hysteresis + relay switch counter, heater pin held OFF). Zero new
  wiring needed; demo = breathe on the DHT22.
- [M] control_v2 flashed: Wi-Fi AP "microaqila" (WPA2, pass aqila1234) UP at
  192.168.4.1, web server serving embedded control page (live temp/humidity,
  AUTO / force-ON / force-OFF, editable thresholds with validation, switch
  counter). Non-blocking millis control loop @2.5 s. Serial verify: DHT 31.8 C
  reading fine alongside WiFi, heap 47080 stable, fan off in auto (< 32.5).
  Defaults: ON>32.5, OFF<32.0. Thresholds are RAM-only for now (reset ->
  defaults); persistence planned with heater firmware. Manual force-ON must
  get a timeout guard before heater channel ever joins the web interface
  (noted in code).
- This IS the proposal's "Mobile IoT Interface" milestone, first version.

## 2026-08-27 session 5: K1 terminal ID + heater build

- User cleared the plastic tub (build had been assembled inside a sealed
  bowl - flagged as unsafe once a 26 W resistor bank joins; tub demoted to
  removable demo-time chamber over sensor+heater only, electronics outside).
- diag_v6 retargeted K2->K1 and reflashed. Readback with the probe wires
  still in place from the K2 round: pair CLOSED when de-energized, OPEN when
  energized, stable over 3 cycles => this pair = COM+NC.
- IMPORTANT: this is the SAME physical screw pair that read COM+NO for K2,
  now reading COM+NC for K1 - the wires are still in K2's trio while K1 is
  the relay being clicked. A K2-trio pair cannot change state when K1 clicks,
  so the wires must actually be in K1's trio (user moved them, or my earlier
  left/right naming mapped to the other trio). Either way the ELECTRICAL fact
  stands on its own and needs no orientation assumption:
    * the pair currently wired is COM+NC for the relay being clicked (K1)
    * => the THIRD screw of that trio is the NO the heater must use.
- Heater rule unchanged and now concrete: heater goes on COM + NO so that
  idle/reset/crash = heater OFF. NC is the screw that is live when the system
  is dead - never use it.
- [M] *** K1 TERMINALS IDENTIFIED *** After moving the probe wires to the
  other pair: open when de-energized, CLOSED when energized => PAIR=COM+NO.
  The two screws now holding the orange probe wires on K1 are the heater
  pair; the remaining (empty) screw is NC and is never used. Fail-safe
  confirmed by construction: idle/reset/crash = heater circuit open.
- Both relay channels now fully characterised (K2 fan pair, K1 heater pair).
- Next: build the 4x22ohm parallel bank, then wire jack+ -> fuse -> K1 COM,
  K1 NO -> bank -> jack-.
- [M] *** HEATER WORKS *** diag_v7 run with 12 V live: cold-check phase held
  31->32.2 C flat (no bypass), then 30 s burst gave a MONOTONIC rise
  32.2 -> 32.8 C (+0.6 C) with humidity falling 77.3 -> 75.2 % (independent
  confirmation - warming air). Post-burst coast continued to 33.2 C = thermal
  lag from the ceramic mass; this is the overshoot mechanism the adaptive
  algorithm must handle, now measured on our own rig.
- [M] User reports resistors get VERY hot in ~1 min and gave off a smell on
  first heat; tape checked and intact, saucer (ceramic) fine => normal
  burn-in of new ceramic resistors, not melting. 26 W is nonetheless more
  than this job needs.
- OPEN SAFETY ITEM: thermal switch STILL NOT WIRED. Must be in the loop
  before any unattended running.
- PROPOSED: rewire bank series-parallel (2 series pairs in parallel) = 22 ohm,
  0.55 A, 6.5 W total, 1.6 W per resistor - a quarter of the per-resistor
  heat, no smell/melt risk, still ample inside a closed chamber.
- [M] *** HEATER NOW UNDER RELAY CONTROL *** Debug chain tonight:
  1. Found heater BYPASSING relays (bank red shared a screw with the 12 V
     feed) -> heater was permanently on whenever wall was on; explains the
     "very hot in 1 min" and the ~0.9 C rise during the earlier cold check.
  2. Rewired so each relay has 12 V on one screw and its load on another.
     Cold check then FLAT 31.3 C for 60 s (bypass gone), but burst gave 0.0.
  3. Bypassed thermal switch -> still 0.0, so switch was not the blocker.
  4. Fan-still-spins test (shared fuse) proved fuse + 12 V healthy => the
     fault was K1's screw pair: 1st and 2nd are both OUTER contacts and
     never connect. Moved bank red to K1 3rd screw.
  5. Result: start 33.3 -> peak 33.7 C (+0.4) during burst, humidity 71.4 ->
     70.9, coast to 33.9 after off. Cold check flat. CONTROL CONFIRMED.
- [M] Screw map now settled: K2 pair = screws 1+2 (fan). K1 pair = screws
  2+3 (heater), middle screw is the shared/COM contact on both channels.
- [M] Room rose 31.3 -> 33.9 C over the evening from the bypassed heater -
  incidental proof the bank has real heating power in open air.
- OPEN: thermal switch is BYPASSED (out of circuit). Its NC/NO type was
  never confirmed - beep-test with the UT33D+ before refitting. Until it is
  back in, the heater must not run unattended; fuse + firmware limits only.
- [M] *** THERMAL SWITCH VERIFIED + FITTED *** Refitted in series (K1 3rd
  screw -> switch -> bank red) and the burst still heated (31.6 -> 31.8,
  coast to 32.1) => current passes when cold => KSD9700 is the NORMALLY-
  CLOSED type. Correct part, protection now live.
- Mounting corrected: switch NOT taped to a resistor body (PVC tape softens
  ~80-100 C, resistor faces run 150 C+, and a 105 C switch on a 150 C surface
  would nuisance-trip constantly). Now taped to the SAUCER beside the bank
  with its shiny sensing face against a resistor edge - senses runaway, not
  normal operating temperature.
- *** HARDWARE BUILD COMPLETE *** sensor, OLED, 2 relays, fan, heater bank,
  fuse, thermal cutout, Wi-Fi web UI: all verified working under ESP control.
- Measurement note for the report: open-air bursts give only +0.2..0.6 C in
  30 s because room air carries the heat away (room fan made it worse). The
  demo chamber (bowl over sensor+bank) is what makes the loop visibly fast.
- User request: HEATER_MAX_ON_MS cut 5 min -> 3 min after smelling a
  "hot clothes iron" odour on a long run and pulling the wall plug. Good
  instinct; limit tightened in control_v3/config.h.
- Q&A logged: dropping 4 resistors to 2 does NOT reduce the smell. Each
  resistor in a PARALLEL bank sees the full 12 V across its own 22 ohm, so
  it dissipates 6.5 W whether there are 2 or 4 of them - surface temperature
  is identical, only total output halves. Cooling each resistor requires
  SERIES wiring (2 in series = 44 ohm, 0.27 A, 1.6 W each).
- Decision: keep the local AP web UI, not Blynk - no internet dependency at
  the AIUB demo, no account/cloud, already working, and it still satisfies
  the proposal's "Mobile IoT Interface".

## 2026-08-29 session 6: adaptive firmware (control_v4)

- Wrote `firmware/control_v4` - control_v3 plus the adaptive band, which is
  the objective the project title is named after. Everything proven in
  sessions 1-5 (pin map, relay polarity, screw map, safety chain) is
  unchanged; only the band handling is new.
- THE RULE, entire algorithm: every ADAPT_WINDOW_MS (60 s) count relay
  switches since the last review. >= ADAPT_BUSY_SW (4) -> widen the band by
  ADAPT_STEP_C (0.1); exactly 0 switches while both relays are idle ->
  tighten by the same step. Clamped to [GAP_MIN_C 0.2, GAP_MAX_C 2.0].
- Split base vs live band: `cfg.gap` is the base the user saves (EEPROM),
  `gap` is the live one adaptation moves. FIXED mode snaps back to `cfg.gap`,
  so it is a true control condition rather than a frozen adapted value.
- FIXED mode still counts each window (it just never acts on the count) so
  both modes are measured identically.
- Added to the web UI: ADAPTIVE/FIXED buttons, live band readout, run-minutes,
  and RESET COUNTERS for starting a clean timed run. OLED gained band + mode
  + elapsed minutes. New endpoint `/api/reset`.
- EEPROM_MAGIC bumped 0xA9C1 -> 0xA9C2 (Settings struct gained `adaptive`),
  so a v3 EEPROM is rejected and defaults are rewritten on first v4 boot.
- [M] Compiled 27% flash / 37% RAM. USER FLASHED IT PERSONALLY from Arduino
  IDE 2.3.10 (sketchbook repointed to C:\Users\AUsrat\.arduino-user so the
  IDE sees the libraries installed by arduino-cli). Upload ended in
  "Hard resetting" = success.
- [M] Serial verify after the user's own upload: banner control_v4, chip
  0x006BE263, SSD1306 OK, AP 'microaqila' UP at 192.168.4.1, heap 46144,
  "Loaded from EEPROM: target 35.0 base gap 0.5 mode ADAPTIVE", band 0.50
  -> heat below 34.5 / fan above 35.5. Heater engaged correctly at 31.0 C.
- [M] User then set target 30.0 from the phone; at 30.8 C the FAN engaged
  (above 30.5) - both directions of the band now confirmed on hardware.
  Heap 44624 with a web client attached.
- Wrote `tools/run_log.py`: resets the board, captures a timed run, and
  writes data/run_<MODE>_<mins>min_<stamp>.{log,csv} plus the summary line
  (mode, duration, total switches, band start/end, temp range) that goes
  into the report table. Verified end-to-end on a 30 s capture.
- NEXT (objective 6, the Results section): two timed runs, same room, same
  target, same base band - one FIXED, one ADAPTIVE - and compare total
  switches. Nothing left to build; this is measurement, not code.
