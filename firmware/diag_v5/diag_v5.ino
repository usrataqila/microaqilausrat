// diag_v5 - screw-terminal identification helper for microaqila.
// The K1/K2 screw terminals carry no NO/COM/NC silkscreen, so they get
// identified by MULTIMETER CONTINUITY, not by guessing. This holds K2 in a
// known state for PHASE_SECS at a time (OLED shows which, with a countdown)
// while the user beeps out the three K2 screws. Contacts are dry: USB power
// only, no 12 V anywhere. K1 stays OFF the entire time.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);
bool oledOk = false;
float lastT = NAN, lastH = NAN;

void draw(const char *big, int secs) {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(big);
  display.setTextSize(3);
  display.setCursor(0, 20);
  display.printf("%d", secs);
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.println(F("probe K2 screws now"));
  if (!isnan(lastT)) display.printf("%.1fC %.0f%%", lastT, lastH);
  display.display();
}

void phase(bool k2on, const char *title) {
  digitalWrite(PIN_RELAY_K2, k2on ? RELAY_ON : RELAY_OFF);
  Serial.printf("\n=== PHASE: %s for %d s ===\n", title, PHASE_SECS);
  for (int s = PHASE_SECS; s > 0; s--) {
    draw(title, s);
    if (s % 15 == 0) {
      float h = dht.readHumidity(), t = dht.readTemperature();
      if (!isnan(t)) { lastT = t; lastH = h; }
      Serial.printf("  [%s %2ds left] dht %s t=%.1f h=%.1f | heap=%u\n",
                    title, s, isnan(t) ? "FAIL" : "ok", t, h, ESP.getFreeHeap());
    }
    delay(1000);
  }
}

void setup() {
  digitalWrite(PIN_RELAY_K1, RELAY_OFF); pinMode(PIN_RELAY_K1, OUTPUT);
  digitalWrite(PIN_RELAY_K2, RELAY_OFF); pinMode(PIN_RELAY_K2, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F("  microaqila diag_v5 (terminal ID)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n", oledOk ? "OK" : "FAILED");
  dht.begin();
  Serial.println(F("K1 permanently OFF. K2 alternates 60 s OFF / 60 s ON."));
}

void loop() {
  phase(false, "K2 OFF");
  phase(true,  "K2 ON");
}
