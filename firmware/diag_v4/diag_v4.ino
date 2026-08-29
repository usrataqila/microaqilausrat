// diag_v4 - relay click test for microaqila.
// CONTROL SIDE ONLY: nothing on the K1/K2 screw terminals, no 12 V anywhere.
// Exercises both channels on a fixed audible pattern while keeping the DHT22
// and OLED alive, so one serial read shows relays cycling, sensor still
// valid, heap flat, and reset reason (a coil-switching brownout would
// appear here as data, not as a guess).

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);

uint32_t cycle = 0;
bool oledOk = false;
bool k1 = false, k2 = false;
float lastT = NAN, lastH = NAN;

void drawStatus() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.printf("K1:%s\n", k1 ? "ON" : "--");
  display.printf("K2:%s\n", k2 ? "ON" : "--");
  display.setTextSize(1);
  display.setCursor(0, 46);
  if (!isnan(lastT)) display.printf("%.1fC %.0f%%RH  ", lastT, lastH);
  else               display.print(F("dht: n/a  "));
  display.printf("cyc %lu", (unsigned long)cycle);
  display.display();
}

void setRelay(uint8_t pin, bool on, bool *state, const char *name) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
  *state = on;
  Serial.printf(">> %s %s (pin driven %s)\n", name, on ? "ON " : "off",
                (on ? RELAY_ON : RELAY_OFF) == LOW ? "LOW/0V" : "HIGH/3.3V");
  drawStatus();
}

void setup() {
  // Relays first, forced OFF, before anything else runs.
  digitalWrite(PIN_RELAY_K1, RELAY_OFF); pinMode(PIN_RELAY_K1, OUTPUT);
  digitalWrite(PIN_RELAY_K2, RELAY_OFF); pinMode(PIN_RELAY_K2, OUTPUT);

  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F("  microaqila diag_v4  (relay test)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
  Serial.printf("K1 on D6(GPIO%d), K2 on D7(GPIO%d), assumed LOW-trigger\n",
                PIN_RELAY_K1, PIN_RELAY_K2);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n", oledOk ? "OK" : "FAILED");
  dht.begin();
  Serial.printf("Free heap:    %u bytes\n\n", ESP.getFreeHeap());

  Serial.println(F("SETTLE 8 s: both relays held OFF - module LEDs must be"));
  Serial.println(F("DARK and there must be ZERO clicks in this window.\n"));
  drawStatus();
  for (int i = 8; i > 0; i--) { Serial.printf("  settle %d\n", i); delay(1000); }
  Serial.println();
}

void loop() {
  Serial.printf("--- cycle %lu ---\n", (unsigned long)cycle);
  setRelay(PIN_RELAY_K1, true,  &k1, "K1"); delay(1200);
  setRelay(PIN_RELAY_K1, false, &k1, "K1"); delay(1500);
  setRelay(PIN_RELAY_K2, true,  &k2, "K2"); delay(1200);
  setRelay(PIN_RELAY_K2, false, &k2, "K2"); delay(500);

  float t = NAN, h = NAN;
  for (uint8_t i = 0; i < DHT_RETRIES; i++) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (!isnan(t) && !isnan(h)) break;
    delay(2100);
  }
  if (!isnan(t)) { lastT = t; lastH = h; }
  Serial.printf("[cyc %lu] dht %s t=%.1f h=%.1f | heap=%u\n",
                (unsigned long)cycle, isnan(t) ? "FAIL" : "ok", t, h,
                ESP.getFreeHeap());
  drawStatus();
  cycle++;
  delay(3500);
}
