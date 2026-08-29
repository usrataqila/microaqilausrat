// control_v2 - fan thermostat + Wi-Fi web control for microaqila.
// The ESP runs its own hotspot (AP_SSID) and serves a phone-friendly page at
// http://192.168.4.1 with live readings, AUTO / force-ON / force-OFF, and
// editable thresholds. Control loop is non-blocking (millis-scheduled) so the
// web server stays responsive. K1 (heater, not wired) is held OFF always.
// NOTE for the future heater build: manual force-ON must gain a timeout guard
// before it is ever allowed to drive the heater channel.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);
ESP8266WebServer server(80);

float t = NAN, h = NAN;
float onC = FAN_ON_C, offC = FAN_OFF_C;
uint8_t mode = 0;                 // 0=auto 1=forced-on 2=forced-off
bool fanOn = false, oledOk = false;
uint32_t switches = 0, ticks = 0, dhtFails = 0, lastTick = 0;

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>microaqila</title><style>
body{font-family:system-ui,sans-serif;background:#10121a;color:#eef;
margin:0;padding:18px;max-width:420px;margin-inline:auto;text-align:center}
h1{font-size:1.1rem;letter-spacing:2px;color:#8af;margin:6px 0 14px}
#t{font-size:4.2rem;font-weight:700;line-height:1}
#h{color:#9ab;margin:4px 0 10px}
#fan{font-size:1.3rem;font-weight:700;margin:10px 0;padding:10px;
border-radius:12px;background:#1a1e2e}
#fan.on{background:#0a4d2e;color:#6f6}
.row{display:flex;gap:8px;margin:14px 0}
button{flex:1;padding:14px 0;font-size:1rem;font-weight:700;border:0;
border-radius:12px;background:#232840;color:#eef}
button.act{background:#2f6fed}
.card{background:#1a1e2e;border-radius:12px;padding:14px;margin:14px 0;text-align:left}
label{display:block;color:#9ab;font-size:.85rem;margin:8px 0 2px}
input{width:100%;box-sizing:border-box;padding:10px;font-size:1.1rem;border:0;
border-radius:8px;background:#10121a;color:#eef}
#save{margin-top:12px;background:#2f6fed}
#msg{min-height:1.2em;color:#fa6;font-size:.9rem;margin-top:8px}
#sw{color:#9ab;font-size:.85rem;margin-top:10px}
</style></head><body>
<h1>MICROAQILA</h1>
<div id="t">--.-&deg;</div><div id="h">--% humidity</div>
<div id="fan">FAN --</div>
<div class="row">
<button id="b0" onclick="setMode('auto')">AUTO</button>
<button id="b1" onclick="setMode('on')">FORCE ON</button>
<button id="b2" onclick="setMode('off')">FORCE OFF</button>
</div>
<div class="card">
<label>Fan ON above (&deg;C)</label><input id="onc" type="number" step="0.1">
<label>Fan OFF below (&deg;C)</label><input id="offc" type="number" step="0.1">
<button id="save" onclick="save()" style="width:100%">SAVE THRESHOLDS</button>
</div>
<div id="sw">switches: -</div><div id="msg"></div>
<script>
let edited=false;
document.getElementById('onc').oninput=()=>edited=true;
document.getElementById('offc').oninput=()=>edited=true;
function setMode(m){fetch('/api/mode?m='+m).then(poll);}
function save(){
 const on=document.getElementById('onc').value,off=document.getElementById('offc').value;
 fetch('/api/set?on='+on+'&off='+off).then(r=>r.text()).then(x=>{
  document.getElementById('msg').textContent=(x=='ok')?'':x; edited=false; poll();});}
function poll(){fetch('/api/status').then(r=>r.json()).then(s=>{
 document.getElementById('t').innerHTML=s.t.toFixed(1)+'&deg;';
 document.getElementById('h').textContent=s.h.toFixed(0)+'% humidity';
 const f=document.getElementById('fan');
 f.textContent='FAN '+(s.fan?'ON':'OFF'); f.className=s.fan?'on':'';
 for(let i=0;i<3;i++)document.getElementById('b'+i).className=(s.mode==i)?'act':'';
 if(!edited){document.getElementById('onc').value=s.on.toFixed(1);
  document.getElementById('offc').value=s.off.toFixed(1);}
 document.getElementById('sw').textContent='switches: '+s.sw;
}).catch(()=>{});}
setInterval(poll,2000);poll();
</script></body></html>)HTML";

void applyFan(bool on) {
  if (on == fanOn) return;
  fanOn = on;
  switches++;
  digitalWrite(PIN_RELAY_K2, on ? RELAY_ON : RELAY_OFF);
  Serial.printf(">>> FAN %s at %.1f C (mode %d, switch #%lu)\n",
                on ? "ON" : "OFF", t, mode, (unsigned long)switches);
}

void handleStatus() {
  char buf[192];
  snprintf(buf, sizeof(buf),
    "{\"t\":%.1f,\"h\":%.1f,\"fan\":%d,\"mode\":%d,\"on\":%.1f,\"off\":%.1f,"
    "\"sw\":%lu,\"up\":%lu}",
    isnan(t) ? -99.9 : t, isnan(h) ? -1.0 : h, fanOn ? 1 : 0, mode, onC, offC,
    (unsigned long)switches, (unsigned long)(millis() / 1000));
  server.send(200, "application/json", buf);
}

void handleSet() {
  float on = server.arg("on").toFloat(), off = server.arg("off").toFloat();
  if (on < 5 || on > 60 || off < 5 || off > 60)
    { server.send(200, "text/plain", "values must be 5-60 C"); return; }
  if (on < off + 0.2)
    { server.send(200, "text/plain", "ON must be above OFF by 0.2+"); return; }
  onC = on; offC = off;
  Serial.printf(">>> thresholds set: on>%.1f off<%.1f\n", onC, offC);
  server.send(200, "text/plain", "ok");
}

void handleMode() {
  String m = server.arg("m");
  if      (m == "auto") mode = 0;
  else if (m == "on")   mode = 1;
  else if (m == "off")  mode = 2;
  Serial.printf(">>> mode -> %d\n", mode);
  server.send(200, "text/plain", "ok");
}

void controlTick() {
  float th = dht.readHumidity(), tt = dht.readTemperature();
  if (!isnan(tt) && !isnan(th)) { t = tt; h = th; }
  else dhtFails++;

  if      (mode == 1) applyFan(true);
  else if (mode == 2) applyFan(false);
  else if (!isnan(t)) {
    if (t > onC)       applyFan(true);
    else if (t < offC) applyFan(false);
    // between offC and onC: hold current state (hysteresis band)
  }

  if (oledOk) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.printf("%.1f", isnan(t) ? 0.0 : t);
    display.setTextSize(1);
    display.print(F("C"));
    display.setTextSize(2);
    display.setCursor(0, 28);
    display.printf("FAN %s %s", fanOn ? "ON" : "--",
                   mode == 0 ? "A" : (mode == 1 ? "M1" : "M0"));
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.printf(">%.1f <%.1f sw%lu\n", onC, offC, (unsigned long)switches);
    display.print(F("192.168.4.1 "));
    display.print(AP_SSID);
    display.display();
  }

  if (ticks % 4 == 0)
    Serial.printf("[%5lu] t=%.1f h=%.1f fan=%s mode=%d sw=%lu heap=%u\n",
                  (unsigned long)ticks, t, h, fanOn ? "ON " : "off", mode,
                  (unsigned long)switches, ESP.getFreeHeap());
  ticks++;
}

void setup() {
  digitalWrite(PIN_RELAY_K1, RELAY_OFF); pinMode(PIN_RELAY_K1, OUTPUT);
  digitalWrite(PIN_RELAY_K2, RELAY_OFF); pinMode(PIN_RELAY_K2, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F(" microaqila control_v2 (web+fan)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
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
  Serial.printf("Web server up. Free heap: %u\n\n", ESP.getFreeHeap());
}

void loop() {
  server.handleClient();
  if (millis() - lastTick >= CTRL_PERIOD_MS) {
    lastTick = millis();
    controlTick();
  }
}
