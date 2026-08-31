// =====================================================================
//  Project G7 - control_v5
//  Threshold-based smart temperature monitoring and control system
//  ESP8266 (NodeMCU) + DHT22 + SSD1306 OLED + 2-channel relay
//
//  WHAT IT DOES
//  Reads the air temperature every 2.5 seconds and takes one of three
//  actions:
//
//      if      (temp < minC)   heater ON,  fan OFF
//      else if (temp > maxC)   heater OFF, fan ON
//      else                    both OFF
//
//  minC and maxC are set by the user from a web page that the ESP itself
//  serves over its own Wi-Fi hotspot, and they are saved to EEPROM so
//  they survive a power cut.
//
//  SAFETY
//  Four checks run before the thermostat is even consulted. Any one of
//  them switches both relays off:
//      1. temperature at or above 40 C          (runaway ceiling)
//      2. sensor has failed 12 reads in a row   (blind - do not heat)
//      3. heater has run for over 3 minutes     (forced rest)
//      4. the user pressed ALL OFF
//
//  Hardware backstops sit underneath all of that and work even if this
//  program stops running: a 3 A fuse, a 105 C thermal cut-out, and relay
//  contacts that spring open the moment they lose power.
// =====================================================================

#include <ESP8266WiFi.h>       // creates the Wi-Fi hotspot
#include <ESP8266WebServer.h>  // serves the control page
#include <EEPROM.h>            // saves settings across reboots
#include <Wire.h>              // I2C bus for the OLED
#include <Adafruit_GFX.h>      // drawing primitives
#include <Adafruit_SSD1306.h>  // the OLED driver itself
#include <DHT.h>               // temperature/humidity sensor
#include "config.h"            // every setting lives next door

// Libraries above, settings next door. Nothing in this file is a magic
// number - if a value needs changing, it is in config.h.


// --------------------------------------------------------------------
//  RELAY POLARITY
// --------------------------------------------------------------------
#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

// Our module is LOW-trigger, so writing LOW to a pin switches its relay
// ON. That is backwards from what most people expect, so we never write
// HIGH or LOW directly in the rest of the program - we write RELAY_ON or
// RELAY_OFF and let these two lines do the translation. If a different
// relay board is ever fitted, only config.h changes.


// --------------------------------------------------------------------
//  OBJECTS AND STATE
// --------------------------------------------------------------------
#define EEPROM_MAGIC 0xA9C3

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);
ESP8266WebServer server(80);

struct Settings { uint16_t magic; float minC; float maxC; };
Settings cfg = { EEPROM_MAGIC, DEFAULT_MIN_C, DEFAULT_MAX_C };

float t = NAN, h = NAN;                      // latest readings
bool heatOn = false, fanOn = false;           // what the relays are doing
bool oledOk = false, autoMode = true;         // display found? auto or off?
uint32_t heatSwitches = 0, fanSwitches = 0;   // how many times each relay moved
uint32_t dhtFails = 0, ticks = 0;
uint32_t lastTick = 0, heatOnSince = 0, restUntil = 0;
const char *lockReason = "";                  // why we are locked off, if we are

// The three hardware objects the program drives, then the variables that
// hold what it currently knows.
//
// NAN means "not a number" - the value we use for a reading we do not
// have yet. It is deliberately not 0, because 0 C is a perfectly valid
// temperature and we must never confuse "cold" with "no reading".
//
// EEPROM_MAGIC is a fingerprint written alongside the saved settings. On
// boot we check for it. If it is missing, the EEPROM holds junk (or an
// older version's layout) and we fall back to the defaults instead of
// loading nonsense.


// --------------------------------------------------------------------
//  THE WEB PAGE
// --------------------------------------------------------------------
const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Project G7</title><style>
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
<h1>PROJECT G7</h1>
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

// The whole phone interface, stored as one block of text inside the
// program. PROGMEM keeps it in flash instead of RAM, which matters on a
// chip with only ~50 KB of RAM free.
//
// The page itself holds no data. Every 2 seconds its JavaScript asks the
// ESP for /api/status and redraws from the answer, so what you see on the
// phone is always what the board actually believes - never a stale copy.
//
// The `edit` flag stops the board overwriting the threshold boxes while
// they are being typed into.


// --------------------------------------------------------------------
//  SAVING AND LOADING SETTINGS
// --------------------------------------------------------------------
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

// EEPROM is a small patch of memory that survives power loss - unlike RAM,
// which forgets everything the moment the board is unplugged.
//
// loadCfg does not trust what it finds. It checks the fingerprint AND
// that the numbers are physically sensible AND that min is below max.
// If any check fails it writes the defaults back instead. That way a
// corrupted byte can never leave the thermostat with impossible
// thresholds it would then try to act on.


// --------------------------------------------------------------------
//  SWITCHING THE RELAYS
// --------------------------------------------------------------------
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

// Every relay change in the whole program goes through these functions -
// nothing writes to a relay pin directly. That gives three things for free:
//
//   1. The first line, `if (on == heatOn) return;`, means asking for a
//      state the relay is already in does nothing at all. The control
//      loop can therefore say "heater on" every 2.5 seconds without the
//      relay actually clicking every 2.5 seconds.
//   2. Every real change is counted and printed, so the serial log is a
//      complete record of what the relays did.
//   3. setHeat stamps heatOnSince the moment the heater comes on, which
//      is what the three-minute limit later measures against.
//
// allOff() is the panic handler: both relays off, and the reason stored
// so it can be shown on the OLED and the phone.


// --------------------------------------------------------------------
//  THE CONTROL LOOP - SAFETY FIRST, THEN THE THERMOSTAT
// --------------------------------------------------------------------
void control() {

  // ---- safety: any one of these overrides the thermostat ----
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

// Read this function top to bottom, because that order IS the safety
// policy. Each check ends in `return`, so if it fires, the thermostat
// below never runs at all. Safety cannot be outvoted by the temperature.
//
//   OVER TEMP    - 40 C or above. Nothing justifies heating past this.
//   SENSOR LOST  - 12 failed reads. We are blind; blind and heating is
//                  the one combination that must never happen.
//   HEATER REST  - either we are inside a forced rest, or the heater has
//                  just exceeded 3 minutes continuous and one starts now.
//   MANUAL OFF   - the user pressed ALL OFF on their phone.
//   NO READING   - no valid measurement yet (only at start-up).
//
// If every check passes, lockReason is cleared and the thermostat runs.
// Three branches, exactly as in the block diagram: too cold -> heat,
// too hot -> cool, otherwise leave everything alone. The heater and fan
// are never on together, because each branch explicitly turns the other
// one off.


// --------------------------------------------------------------------
//  THE OLED
// --------------------------------------------------------------------
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

// Four lines on a 128x64 screen: the temperature in large digits, the two
// thresholds, what the relays are doing, and either a safety message or
// the address to type into a phone.
//
// The screen is redrawn from scratch every cycle rather than patched, so
// what it shows can never drift out of step with the real state.
//
// `if (!oledOk) return;` means a dead or unplugged display cannot take the
// thermostat down with it - the control loop carries on regardless. The
// screen is for the human, not for the machine.


// --------------------------------------------------------------------
//  WEB ENDPOINTS
// --------------------------------------------------------------------
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
  if (mn < USER_MIN_LIMIT_C || mn > USER_MAX_LIMIT_C ||
      mx < USER_MIN_LIMIT_C || mx > USER_MAX_LIMIT_C) {
    server.send(200, "text/plain", "both must be 15-38 C");
    return;
  }
  if (mn >= mx) {
    server.send(200, "text/plain", "min must be below max");
    return;
  }
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

// Three small handlers - the entire interface between the phone and the
// board.
//
//   /api/status  packs the current state into JSON. The phone asks for
//                this every 2 seconds; it never changes anything.
//   /api/set     changes the thresholds - and refuses bad input rather
//                than accepting it. Values must sit inside 15-38 C, and
//                min must be below max. The upper limit is deliberately
//                below the 40 C abort ceiling, so it is impossible to set
//                a fan threshold the safety cut-out would beat to it.
//   /api/mode    switches between AUTO and ALL OFF.
//
// Note what is NOT here: there is no way to force the heater on from the
// phone. Heating is something the thermostat decides, never something a
// person can hold down by hand.


// --------------------------------------------------------------------
//  SETUP - RUNS ONCE AT POWER-UP
// --------------------------------------------------------------------
void setup() {
  digitalWrite(PIN_RELAY_HEAT, RELAY_OFF); pinMode(PIN_RELAY_HEAT, OUTPUT);
  digitalWrite(PIN_RELAY_FAN,  RELAY_OFF); pinMode(PIN_RELAY_FAN,  OUTPUT);

  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("===================================="));
  Serial.println(F(" Project G7 - control_v5"));
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
  Serial.printf("Heater below %.1f, fan above %.1f, nothing in between.\n",
                cfg.minC, cfg.maxC);
  Serial.printf("Abort at %.1f C. Heater max %d s on, %d s rest.\n\n",
                (double)ABORT_TEMP_C, HEATER_MAX_ON_MS / 1000,
                HEATER_REST_MS / 1000);
}

// Runs once, in a deliberate order.
//
// The very first thing that happens - before serial, before anything - is
// forcing both relay pins OFF. Note that digitalWrite comes BEFORE
// pinMode: setting the value first means the pin never spends even one
// instruction as an output at the wrong level. On a board driving a
// heater, that ordering matters.
//
// After that: open serial, restore saved settings, start the display and
// the sensor, raise the Wi-Fi hotspot, register the four web addresses,
// and print a summary of the limits actually in force. That printout is
// the quickest way to confirm the board is running what you think it is.


// --------------------------------------------------------------------
//  LOOP - RUNS FOREVER
// --------------------------------------------------------------------
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

// The main cycle, and the shape of it is the important part.
//
// server.handleClient() runs on EVERY pass, thousands of times a second,
// so the phone always gets an instant response. The measure-and-decide
// work is rationed to once every 2.5 seconds by the millis() check on the
// second line.
//
// That check is why there is no delay() anywhere in this program. delay()
// would freeze the whole chip - the web page would stop responding and
// the Wi-Fi stack would be starved. Instead the loop asks "has enough
// time passed?" and returns immediately if not. The board is therefore
// never busy waiting; it is always available.
//
// A reading is only accepted if BOTH temperature and humidity came back
// valid. A good read resets the failure counter to zero, so only a run of
// consecutive failures - a genuinely dead sensor, not one bad sample -
// can trip the SENSOR LOST cut-out.
//
// Then control() decides, drawOled() shows it, and every fourth pass
// (~10 s) one line goes to the serial log.
