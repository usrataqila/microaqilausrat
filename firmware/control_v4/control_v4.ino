// control_v4 - microaqila ADAPTIVE thermostat.
//
// Same thermostat as control_v3, with one thing added: the hysteresis band
// tunes itself. That is the whole point of the project.
//
// THE ADAPTIVE RULE, in one sentence:
//   every minute, look at how many times the relays switched; if that was a
//   lot, widen the band; if nothing switched and we were sitting idle,
//   tighten it back.
//
// A FIXED mode is included so the same board can run both ways under the
// same conditions. That is what makes the comparison in the report fair:
// run fixed, count switches; run adaptive, count switches; same room, same
// target, same hardware.
//
// Safety, in order of precedence - all of these force BOTH relays off:
//   1. temp >= ABORT_TEMP_C          (runaway ceiling)
//   2. DHT failed DHT_FAIL_LIMIT x   (blind: never heat what we cannot see)
//   3. heater on > HEATER_MAX_ON_MS  (forced rest, guards a stuck demand)
// Hardware backstops beyond these: 3 A fuse and a 105 C thermal cutout in
// series with the bank, plus LOW-trigger relays that open on reset/crash.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)
#define EEPROM_MAGIC 0xA9C2   // bumped from v3: the settings struct changed

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);
ESP8266WebServer server(80);

// Saved settings. cfg.gap is the BASE band the user set; `gap` below is the
// live one the adaptive rule moves around. Fixed mode always runs at cfg.gap.
struct Settings { uint16_t magic; float target; float gap; uint8_t adaptive; };
Settings cfg = { EEPROM_MAGIC, DEFAULT_TARGET_C, DEFAULT_GAP_C, DEFAULT_ADAPTIVE };

float t = NAN, h = NAN;
float gap = DEFAULT_GAP_C;                  // live band, half-width in C
bool heatOn = false, fanOn = false, oledOk = false, autoMode = true;
bool adaptive = DEFAULT_ADAPTIVE;
uint32_t heatSwitches = 0, fanSwitches = 0, dhtFails = 0, ticks = 0;
uint32_t lastTick = 0, heatOnSince = 0, restUntil = 0;
uint32_t lastAdapt = 0, swAtLastAdapt = 0, runStart = 0;
const char *lockReason = "";

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>microaqila</title><style>
body{font-family:system-ui,sans-serif;background:#10121a;color:#eef;margin:0;
padding:18px;max-width:420px;margin-inline:auto;text-align:center}
h1{font-size:1rem;letter-spacing:3px;color:#8af;margin:6px 0 14px}
#t{font-size:4rem;font-weight:700;line-height:1}
#sub{color:#9ab;margin:2px 0 14px;font-size:.9rem}
.pills{display:flex;gap:8px;margin:12px 0}
.pill{flex:1;padding:12px 4px;border-radius:12px;background:#1a1e2e;
font-weight:700;font-size:.95rem}
.pill.heat{background:#5a1f0a;color:#fb8}
.pill.fan{background:#0a3d4d;color:#6df}
.row{display:flex;gap:8px;margin:8px 0}
button{flex:1;padding:13px 0;font-size:.95rem;font-weight:700;border:0;
border-radius:12px;background:#232840;color:#eef}
button.act{background:#2f6fed}
.card{background:#1a1e2e;border-radius:12px;padding:14px;margin:12px 0;
text-align:left}
label{display:block;color:#9ab;font-size:.8rem;margin:6px 0 3px}
input{width:100%;box-sizing:border-box;padding:10px;font-size:1.1rem;border:0;
border-radius:8px;background:#10121a;color:#eef}
#save{margin-top:10px;background:#2f6fed;width:100%}
#msg{min-height:1.1em;color:#fa6;font-size:.85rem}
#lock{color:#f77;font-weight:700;min-height:1.1em;margin:6px 0}
#band{font-size:1.6rem;font-weight:700;color:#8af;margin:2px 0}
#run,#stats{color:#9ab;font-size:.85rem;margin:3px 0}
#rst{margin-top:10px;width:100%;background:#3a2030;color:#fbb}
</style></head><body>
<h1>MICROAQILA</h1>
<div id="t">--.-&deg;</div>
<div id="sub">humidity --% &middot; target --&deg;</div>
<div id="lock"></div>
<div class="pills">
<div class="pill" id="ph">HEATER --</div>
<div class="pill" id="pf">FAN --</div>
</div>
<div class="row">
<button id="b0" onclick="mode('auto')">AUTO</button>
<button id="b1" onclick="mode('off')">ALL OFF</button>
</div>
<div class="row">
<button id="b2" onclick="mode('adaptive')">ADAPTIVE</button>
<button id="b3" onclick="mode('fixed')">FIXED</button>
</div>
<div class="card">
<label>Live band (this is what adapts)</label>
<div id="band">&plusmn;--&deg;</div>
<div id="run">run -- min</div>
<div id="stats">switches: heater - / fan - &middot; total -</div>
<button id="rst" onclick="reset()">RESET COUNTERS</button>
</div>
<div class="card">
<label>Target temperature (&deg;C)</label><input id="tg" type="number" step="0.5">
<label>Starting band (&deg;C)</label><input id="gp" type="number" step="0.1">
<button id="save" onclick="save()">SAVE</button>
<div id="msg"></div>
</div>
<script>
let edit=false;
tg.oninput=()=>edit=true; gp.oninput=()=>edit=true;
function mode(m){fetch('/api/mode?m='+m).then(poll);}
function reset(){fetch('/api/reset').then(poll);}
function save(){fetch('/api/set?target='+tg.value+'&gap='+gp.value)
 .then(r=>r.text()).then(x=>{msg.textContent=(x=='ok')?'saved':x;edit=false;poll();});}
function poll(){fetch('/api/status').then(r=>r.json()).then(s=>{
 t.innerHTML=(s.t>-90?s.t.toFixed(1):'--.-')+'&deg;';
 sub.innerHTML='humidity '+(s.h>=0?s.h.toFixed(0):'--')+'% &middot; target '
   +s.target.toFixed(1)+'&deg;';
 ph.textContent='HEATER '+(s.heat?'ON':'off'); ph.className='pill'+(s.heat?' heat':'');
 pf.textContent='FAN '+(s.fan?'ON':'off');    pf.className='pill'+(s.fan?' fan':'');
 lock.textContent=s.lock||'';
 b0.className=s.auto?'act':''; b1.className=s.auto?'':'act';
 b2.className=s.adapt?'act':''; b3.className=s.adapt?'':'act';
 band.innerHTML='&plusmn;'+s.gap.toFixed(2)+'&deg; '+(s.adapt?'adaptive':'fixed');
 run.textContent='run '+s.mins+' min';
 stats.textContent='switches: heater '+s.hsw+' / fan '+s.fsw
   +' · total '+(s.hsw+s.fsw);
 if(!edit){tg.value=s.target.toFixed(1);gp.value=s.basegap.toFixed(1);}
}).catch(()=>{});}
setInterval(poll,2000);poll();
</script></body></html>)HTML";

void saveCfg() {
  EEPROM.put(0, cfg);
  EEPROM.commit();
  Serial.printf(">>> saved: target %.1f base gap %.1f mode %s\n",
                cfg.target, cfg.gap, cfg.adaptive ? "ADAPTIVE" : "FIXED");
}

void loadCfg() {
  Settings s;
  EEPROM.get(0, s);
  if (s.magic == EEPROM_MAGIC && s.target > 5 && s.target < 60 &&
      s.gap > 0.05 && s.gap < 10) {
    cfg = s;
    Serial.printf("Loaded from EEPROM: target %.1f base gap %.1f mode %s\n",
                  cfg.target, cfg.gap, cfg.adaptive ? "ADAPTIVE" : "FIXED");
  } else {
    Serial.println(F("EEPROM empty/invalid -> using defaults"));
    saveCfg();
  }
  gap = cfg.gap;
  adaptive = cfg.adaptive;
}

void setHeat(bool on) {
  if (on == heatOn) return;
  heatOn = on;
  heatSwitches++;
  digitalWrite(PIN_RELAY_HEAT, on ? RELAY_ON : RELAY_OFF);
  if (on) heatOnSince = millis();
  Serial.printf(">>> HEATER %s at %.1f C (switch #%lu)\n",
                on ? "ON" : "off", t, (unsigned long)heatSwitches);
}

void setFan(bool on) {
  if (on == fanOn) return;
  fanOn = on;
  fanSwitches++;
  digitalWrite(PIN_RELAY_FAN, on ? RELAY_ON : RELAY_OFF);
  Serial.printf(">>> FAN %s at %.1f C (switch #%lu)\n",
                on ? "ON" : "off", t, (unsigned long)fanSwitches);
}

void allOff(const char *why) {
  setHeat(false);
  setFan(false);
  if (lockReason != why) Serial.printf("!!! ALL OFF: %s\n", why);
  lockReason = why;
}

// ---------------------------------------------------------------------------
// THE ADAPTIVE RULE. Runs once per ADAPT_WINDOW_MS, and that is the whole
// algorithm - a counter and two ifs.
//
//   switched a lot this window  -> the band is too tight, widen it
//   switched nothing, sitting idle -> we have room to spare, tighten it
//
// Widening cuts switching because the temperature must travel further before
// the next relay event. Tightening gives back accuracy when the room is calm.
// ---------------------------------------------------------------------------
void adapt() {
  if (millis() - lastAdapt < ADAPT_WINDOW_MS) return;
  lastAdapt = millis();

  uint32_t total = heatSwitches + fanSwitches;
  uint32_t inWindow = total - swAtLastAdapt;
  swAtLastAdapt = total;

  // Fixed mode still counts the window (so the two modes are measured the
  // same way) but never moves the band. That is the control condition.
  if (!adaptive) return;

  const float before = gap;
  if (inWindow >= ADAPT_BUSY_SW) {
    gap += ADAPT_STEP_C;
  } else if (inWindow == 0 && !heatOn && !fanOn) {
    gap -= ADAPT_STEP_C;
  }
  if (gap < GAP_MIN_C) gap = GAP_MIN_C;
  if (gap > GAP_MAX_C) gap = GAP_MAX_C;

  if (gap != before)
    Serial.printf("~~~ ADAPT: %lu switches last window -> band %.2f -> %.2f\n",
                  (unsigned long)inWindow, before, gap);
}

void setAdaptive(bool on) {
  adaptive = on;
  if (!on) gap = cfg.gap;      // fixed mode always runs at the base band
  lastAdapt = millis();
  swAtLastAdapt = heatSwitches + fanSwitches;
  Serial.printf(">>> mode: %s (band %.2f)\n", on ? "ADAPTIVE" : "FIXED", gap);
}

// Zero the counters and start a fresh measurement run. This is what you press
// before each timed run when collecting the numbers for the report.
void resetRun() {
  heatSwitches = fanSwitches = swAtLastAdapt = 0;
  runStart = lastAdapt = millis();
  gap = cfg.gap;
  Serial.printf("=== RUN RESET: %s, target %.1f, band %.2f ===\n",
                adaptive ? "ADAPTIVE" : "FIXED", cfg.target, gap);
}

void control() {
  // ---- safety first: any of these wins over the thermostat ----
  if (!isnan(t) && t >= ABORT_TEMP_C)      { allOff("OVER TEMP");   return; }
  if (dhtFails >= DHT_FAIL_LIMIT)          { allOff("SENSOR LOST"); return; }
  if (millis() < restUntil)                { allOff("HEATER REST"); return; }
  if (heatOn && millis() - heatOnSince > HEATER_MAX_ON_MS) {
    restUntil = millis() + HEATER_REST_MS;
    allOff("HEATER REST");
    return;
  }
  if (!autoMode)                           { allOff("MANUAL OFF");  return; }
  if (isnan(t))                            { allOff("NO READING");  return; }

  lockReason = "";
  const float lo = cfg.target - gap;
  const float hi = cfg.target + gap;

  // Below the band -> heat. Above -> cool. Inside -> hold whatever we were
  // doing, which is what stops the relay chattering at the setpoint.
  if (t < lo) { setFan(false);  setHeat(true);  }
  else if (t > hi) { setHeat(false); setFan(true); }
  else if (t >= cfg.target && heatOn) setHeat(false);
  else if (t <= cfg.target && fanOn)  setFan(false);
}

void drawOled() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(0, 0);
  if (isnan(t)) display.print(F("--.-"));
  else          display.printf("%.1f", t);
  display.setTextSize(1);
  display.print(F("C"));

  display.setTextSize(1);
  display.setCursor(0, 26);
  display.printf("T%.1f %c%.2f %s", cfg.target, 0x18, gap,
                 adaptive ? "ADAPT" : "FIXED");
  display.setTextSize(2);
  display.setCursor(0, 36);
  display.printf("%s %s", heatOn ? "HEAT" : "----", fanOn ? "FAN" : "---");
  display.setTextSize(1);
  display.setCursor(0, 54);
  if (lockReason[0]) display.print(lockReason);
  else display.printf("sw %lu  %lum  192.168.4.1",
                      (unsigned long)(heatSwitches + fanSwitches),
                      (unsigned long)((millis() - runStart) / 60000));
  display.display();
}

void handleStatus() {
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"t\":%.1f,\"h\":%.1f,\"heat\":%d,\"fan\":%d,\"auto\":%d,\"adapt\":%d,"
    "\"target\":%.1f,\"gap\":%.2f,\"basegap\":%.1f,"
    "\"hsw\":%lu,\"fsw\":%lu,\"mins\":%lu,\"lock\":\"%s\"}",
    isnan(t) ? -99.9 : t, isnan(h) ? -1.0 : h, heatOn ? 1 : 0, fanOn ? 1 : 0,
    autoMode ? 1 : 0, adaptive ? 1 : 0, cfg.target, gap, cfg.gap,
    (unsigned long)heatSwitches, (unsigned long)fanSwitches,
    (unsigned long)((millis() - runStart) / 60000), lockReason);
  server.send(200, "application/json", buf);
}

void handleSet() {
  float target = server.arg("target").toFloat();
  float g = server.arg("gap").toFloat();
  if (target < 20 || target > 45)
    { server.send(200, "text/plain", "target must be 20-45 C"); return; }
  if (g < 0.1 || g > 5)
    { server.send(200, "text/plain", "band must be 0.1-5 C"); return; }
  cfg.target = target;
  cfg.gap = g;
  gap = g;                       // a new base band restarts adaptation from it
  saveCfg();
  server.send(200, "text/plain", "ok");
}

void handleMode() {
  String m = server.arg("m");
  if (m == "auto")          autoMode = true;
  else if (m == "off")      autoMode = false;
  else if (m == "adaptive") { setAdaptive(true);  cfg.adaptive = 1; saveCfg(); }
  else if (m == "fixed")    { setAdaptive(false); cfg.adaptive = 0; saveCfg(); }
  if (m == "auto" || m == "off")
    Serial.printf(">>> mode: %s\n", autoMode ? "AUTO" : "ALL OFF");
  server.send(200, "text/plain", "ok");
}

void handleReset() {
  resetRun();
  server.send(200, "text/plain", "ok");
}

void setup() {
  digitalWrite(PIN_RELAY_HEAT, RELAY_OFF); pinMode(PIN_RELAY_HEAT, OUTPUT);
  digitalWrite(PIN_RELAY_FAN,  RELAY_OFF); pinMode(PIN_RELAY_FAN,  OUTPUT);

  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("======================================"));
  Serial.println(F(" microaqila control_v4 (adaptive)"));
  Serial.println(F("======================================"));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());

  EEPROM.begin(64);
  loadCfg();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n", oledOk ? "OK" : "FAILED");
  dht.begin();

  WiFi.mode(WIFI_AP);
  bool ap = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP '%s': %s, IP %s\n", AP_SSID, ap ? "UP" : "FAILED",
                WiFi.softAPIP().toString().c_str());
  server.on("/", []() { server.send_P(200, "text/html", PAGE); });
  server.on("/api/status", handleStatus);
  server.on("/api/set", handleSet);
  server.on("/api/mode", handleMode);
  server.on("/api/reset", handleReset);
  server.begin();
  Serial.printf("Web server up. Heap %u\n", ESP.getFreeHeap());
  Serial.printf("Mode %s, band %.2f: heat below %.1f, fan above %.1f\n",
                adaptive ? "ADAPTIVE" : "FIXED", gap,
                cfg.target - gap, cfg.target + gap);
  Serial.printf("Adaptive review every %d s: >=%d switches widens, "
                "0 switches while idle tightens, step %.2f C.\n\n",
                ADAPT_WINDOW_MS / 1000, ADAPT_BUSY_SW, (double)ADAPT_STEP_C);

  runStart = lastAdapt = millis();
}

void loop() {
  server.handleClient();
  if (millis() - lastTick < CTRL_PERIOD_MS) return;
  lastTick = millis();

  float th = dht.readHumidity(), tt = dht.readTemperature();
  if (!isnan(tt) && !isnan(th)) { t = tt; h = th; dhtFails = 0; }
  else dhtFails++;

  control();
  adapt();
  drawOled();

  // One data line every ~10 s. This is the record to screenshot for the
  // report: mode, live band, switch counts, minutes elapsed.
  if (ticks % 4 == 0)
    Serial.printf("[%4lum] t=%.1f h=%.1f heat=%s fan=%s band=%.2f %s "
                  "sw=%lu/%lu tot=%lu %s\n",
                  (unsigned long)((millis() - runStart) / 60000), t, h,
                  heatOn ? "ON " : "off", fanOn ? "ON " : "off", gap,
                  adaptive ? "ADAPT" : "FIXED",
                  (unsigned long)heatSwitches, (unsigned long)fanSwitches,
                  (unsigned long)(heatSwitches + fanSwitches), lockReason);
  ticks++;
}
