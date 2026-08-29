// diag_v3 - DHT22 sensor test for microaqila.
// Reads temperature/humidity, shows them live on the OLED, and reports a
// running pass/fail tally over serial. The tally matters: a DHT22 that reads
// once and then NAKs half the time is a wiring/pull-up problem, and averages
// alone would hide it.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);

uint32_t attempts = 0, ok = 0, fails = 0;
float tMin = NAN, tMax = NAN;
bool oledOk = false;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F("  microaqila diag_v3  (DHT22)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
  Serial.printf("DHT on:       D5 (GPIO%d), type DHT22\n", PIN_DHT);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n", oledOk ? "OK" : "FAILED");

  dht.begin();
  Serial.printf("Free heap:    %u bytes\n\n", ESP.getFreeHeap());
  Serial.println(F("t=temp C  h=humidity %  (first read often fails - normal)\n"));
}

void show(float t, float h, bool valid) {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (valid) {
    display.setTextSize(3);
    display.setCursor(0, 2);
    display.printf("%.1f", t);
    display.setTextSize(1);
    display.print(F(" C"));

    display.setTextSize(2);
    display.setCursor(0, 30);
    display.printf("%.1f", h);
    display.setTextSize(1);
    display.print(F(" %RH"));
  } else {
    display.setTextSize(2);
    display.setCursor(0, 8);
    display.println(F("DHT FAIL"));
    display.setTextSize(1);
    display.println(F("no valid read"));
  }

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("ok %lu/%lu  fail %lu",
                 (unsigned long)ok, (unsigned long)attempts, (unsigned long)fails);
  display.display();
}

void loop() {
  float t = NAN, h = NAN;
  for (uint8_t i = 0; i < DHT_RETRIES; i++) {
    h = dht.readHumidity();
    t = dht.readTemperature();          // Celsius
    if (!isnan(t) && !isnan(h)) break;
    delay(2100);                        // respect the 2 s minimum between reads
  }
  attempts++;

  const bool valid = !isnan(t) && !isnan(h);
  if (valid) {
    ok++;
    if (isnan(tMin) || t < tMin) tMin = t;
    if (isnan(tMax) || t > tMax) tMax = t;
    Serial.printf("[%4lu] t=%.1f C  h=%.1f %%  | min %.1f max %.1f | ok %lu/%lu | heap=%u\n",
                  (unsigned long)attempts, t, h, tMin, tMax,
                  (unsigned long)ok, (unsigned long)attempts, ESP.getFreeHeap());
  } else {
    fails++;
    Serial.printf("[%4lu] READ FAILED (NaN after %d tries) | ok %lu/%lu | heap=%u\n",
                  (unsigned long)attempts, DHT_RETRIES,
                  (unsigned long)ok, (unsigned long)attempts, ESP.getFreeHeap());
  }
  show(t, h, valid);
  delay(DHT_READ_MS);
}
