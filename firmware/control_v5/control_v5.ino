// control_v5 - microaqila threshold thermostat.
//
// Two user-set thresholds, no hysteresis band:
//
//     if      (temp < minC)  heater ON,  fan OFF
//     else if (temp > maxC)  heater OFF, fan ON
//     else                   both OFF
//
// Thresholds are editable from the ESP's own web page and saved to EEPROM.
//
// Safety, in order of precedence - all of these force BOTH relays off:
//   1. temp >= ABORT_TEMP_C          (runaway ceiling)
//   2. DHT failed DHT_FAIL_LIMIT x   (blind: never heat what we cannot see)
//   3. heater on > HEATER_MAX_ON_MS  (forced rest, guards a stuck demand)
// Hardware backstops beyond these: 3 A fuse and a 105 C thermal cutout in
// series with the heater, plus LOW-trigger relays that open on reset/crash.

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
#define EEPROM_MAGIC 0xA9C3   // bumped: the settings struct changed again

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);
ESP8266WebServer server(80);

struct Settings { uint16_t magic; float minC; float maxC; };
Settings cfg = { EEPROM_MAGIC, DEFAULT_MIN_C, DEFAULT_MAX_C };

float t = NAN, h = NAN;
bool heatOn = false, fanOn = false, oledOk = false, autoMode = true;
uint32_t heatSwitches = 0, fanSwitches = 0, dhtFails = 0, ticks = 0;
uint32_t lastTick = 0, heatOnSince = 0, restUntil = 0;
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
.row{display:flex;gap:8px;margin:12px 0}
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
</style></head><body>
<h1>MICROAQILA</h1>
<div id="t">--.-&deg;</div>
<div id="sub">humidity --%</div>
<div id="lock"></div>
<div class="pills">
<div class="pill" id="ph">HEATER --</div>
<div class="pill" id="pf">FAN --</div>
</div>
<div class="row">
<button id="b0" onclick="mode('auto')">AUTO</button>
<button id="b1" onclick="mode('off')">ALL OFF</button>
</div>
<div class="card">
<label>Min temperature &mdash; heater below this (&deg;C)</label>
<input id="mn" type="number" step="0.5">
<label>Max temperature &mdash; fan above this (&deg;C)</label>
<input id="mx" type="number" step="0.5">
<button id="save" onclick="save()">SAVE</button>
<div id="msg"></div>
</div>
<script>
let edit=false;
mn.oninput=()=>edit=true; mx.oninput=()=>edit=true;
function mode(m){fetch('/api/mode?m='+m).then(poll);}
function save(){fetch('/api/set?min='+mn.value+'&max='+mx.value)
 .then(r=>r.text()).then(x=>{msg.textContent=(x=='ok')?'saved':x;edit=false;poll();});}
function poll(){fetch('/api/status').then(r=>r.json()).then(s=>{
 t.innerHTML=(s.t>-90?s.t.toFixed(1):'--.-')+'&deg;';
 sub.innerHTML='humidity '+(s.h>=0?s.h.toFixed(0):'--')+'%';
 ph.textContent='HEATER '+(s.heat?'ON':'off'); ph.className='pill'+(s.heat?' heat':'');
 pf.textContent='FAN '+(s.fan?'ON':'off');    pf.className='pill'+(s.fan?' fan':'');
 lock.textContent=s.lock||'';
 b0.className=s.auto?'act':''; b1.className=s.auto?'':'act';
 if(!edit){mn.value=s.min.toFixed(1);mx.value=s.max.toFixed(1);}
}).catch(()=>{});}
setInterval(poll,2000);poll();
</script></body></html>)HTML";

void saveCfg() {
  EEPROM.put(0, cfg);
  EEPROM.commit();
  Serial.printf(">>> saved: min %.1f max %.1f\n", cfg.minC, cfg.maxC);
}

void loadCfg() {
  Settings s;
  EEPROM.get(0, s);
  if (s.magic == EEPROM_MAGIC && s.minC > 5 && s.minC < 60 &&
      s.maxC > 5 && s.maxC < 60 && s.minC < s.maxC) {
    cfg = s;
    Serial.printf("Loaded from EEPROM: min %.1f max %.1f\n", cfg.minC, cfg.maxC);
  } else {
    Serial.println(F("EEPROM empty/invalid -> using defaults"));
    saveCfg();
  }
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

  // ---- the thermostat: two thresholds, three outcomes ----
  if (t < cfg.minC)      { setFan(false);  setHeat(true);  }
  else if (t > cfg.maxC) { setHeat(false); setFan(true);   }
  else                   { setHeat(false); setFan(false);  }
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
  display.printf("min %.1f  max %.1f", cfg.minC, cfg.maxC);
  display.setTextSize(2);
  display.setCursor(0, 36);
  display.printf("%s %s", heatOn ? "HEAT" : "----", fanOn ? "FAN" : "---");
  display.setTextSize(1);
  display.setCursor(0, 54);
  if (lockReason[0]) display.print(lockReason);
  else               display.print(F("192.168.4.1"));
  display.display();
}

void handleStatus() {
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"t\":%.1f,\"h\":%.1f,\"heat\":%d,\"fan\":%d,\"auto\":%d,"
    "\"min\":%.1f,\"max\":%.1f,\"lock\":\"%s\"}",
    isnan(t) ? -99.9 : t, isnan(h) ? -1.0 : h, heatOn ? 1 : 0, fanOn ? 1 : 0,
    autoMode ? 1 : 0, cfg.minC, cfg.maxC, lockReason);
  server.send(200, "application/json", buf);
}

void handleSet() {
  float mn = server.arg("min").toFloat();
  float mx = server.arg("max").toFloat();
  if (mn < 15 || mn > 45 || mx < 15 || mx > 45)
    { server.send(200, "text/plain", "both must be 15-45 C"); return; }
  if (mn >= mx)
    { server.send(200, "text/plain", "min must be below max"); return; }
  cfg.minC = mn;
  cfg.maxC = mx;
  saveCfg();
  server.send(200, "text/plain", "ok");
}

void handleMode() {
  autoMode = (server.arg("m") == "auto");
  Serial.printf(">>> mode: %s\n", autoMode ? "AUTO" : "ALL OFF");
  server.send(200, "text/plain", "ok");
}

void setup() {
  digitalWrite(PIN_RELAY_HEAT, RELAY_OFF); pinMode(PIN_RELAY_HEAT, OUTPUT);
  digitalWrite(PIN_RELAY_FAN,  RELAY_OFF); pinMode(PIN_RELAY_FAN,  OUTPUT);

  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("===================================="));
  Serial.println(F(" microaqila control_v5 (thresholds)"));
  Serial.println(F("===================================="));
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
  server.begin();
  Serial.printf("Web server up. Heap %u\n", ESP.getFreeHeap());
  Serial.printf("Heater below %.1f, fan above %.1f, nothing in between.\n\n",
                cfg.minC, cfg.maxC);
}

void loop() {
  server.handleClient();
  if (millis() - lastTick < CTRL_PERIOD_MS) return;
  lastTick = millis();

  float th = dht.readHumidity(), tt = dht.readTemperature();
  if (!isnan(tt) && !isnan(th)) { t = tt; h = th; dhtFails = 0; }
  else dhtFails++;

  control();
  drawOled();

  if (ticks % 4 == 0)
    Serial.printf("[%5lu] t=%.1f h=%.1f heat=%s fan=%s sw=%lu/%lu %s\n",
                  (unsigned long)ticks, t, h, heatOn ? "ON " : "off",
                  fanOn ? "ON " : "off", (unsigned long)heatSwitches,
                  (unsigned long)fanSwitches, lockReason);
  ticks++;
}
