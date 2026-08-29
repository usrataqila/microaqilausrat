// diag_v7 - heater bring-up test for microaqila. SUPERVISED, ONE SHOT.
//
// Phase 1 (COLD_CHECK_MS): heater forced OFF. If the bank warms up during
//   this window the heater is wired around the relay (wrong screw) - the
//   whole point of the phase is to catch that before any burst.
// Phase 2 (BURST_MS): single heater ON burst while the DHT22 is watched.
// Phase 3: heater OFF permanently until the board is reset.
//
// Aborts to permanent-OFF on: temp >= ABORT_TEMP_C, or DHT failure (a dead
// sensor means we are blind, and blind + heater is exactly what must not run).
// Fan stays OFF throughout so the temperature rise is purely the heater's.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);

uint8_t phase = 0;                 // 0=cold check, 1=burst, 2=done/locked
uint32_t phaseStart = 0, lastRead = 0;
float t = NAN, h = NAN, tStart = NAN, tPeak = NAN;
bool heaterOn = false, oledOk = false, aborted = false;
const char *abortWhy = "";

void heater(bool on) {
  if (on == heaterOn) return;
  heaterOn = on;
  digitalWrite(PIN_RELAY_K1, on ? RELAY_ON : RELAY_OFF);
  Serial.printf(">>> HEATER %s at %.1f C\n", on ? "ON" : "OFF", t);
}

void lockOff(const char *why) {
  heater(false);
  phase = 2;
  aborted = true;
  abortWhy = why;
  Serial.printf("!!! ABORT: %s - heater locked OFF until reset\n", why);
}

void draw(uint32_t secsLeft) {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.printf("%.1fC\n", isnan(t) ? 0.0 : t);
  display.setTextSize(1);
  if (phase == 0) display.printf("COLD CHECK  %lus\n", (unsigned long)secsLeft);
  else if (phase == 1) display.printf("HEATING     %lus\n", (unsigned long)secsLeft);
  else display.printf("%s\n", aborted ? abortWhy : "TEST COMPLETE");
  display.printf("heater %s\n", heaterOn ? "ON" : "off");
  if (!isnan(tStart) && !isnan(tPeak))
    display.printf("start %.1f peak %.1f", tStart, tPeak);
  display.display();
}

void setup() {
  digitalWrite(PIN_RELAY_K1, RELAY_OFF); pinMode(PIN_RELAY_K1, OUTPUT);
  digitalWrite(PIN_RELAY_K2, RELAY_OFF); pinMode(PIN_RELAY_K2, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F(" microaqila diag_v7 (heater test)"));
  Serial.println(F("=================================="));
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n", oledOk ? "OK" : "FAILED");
  dht.begin();
  Serial.printf("PHASE 1: heater OFF for %d s - bank MUST stay cold.\n",
                COLD_CHECK_MS / 1000);
  Serial.printf("PHASE 2: one %d s burst. Abort if temp >= %.1f C.\n\n",
                BURST_MS / 1000, ABORT_TEMP_C);
  phaseStart = millis();
}

void loop() {
  if (millis() - lastRead >= 2000) {
    lastRead = millis();
    float th = dht.readHumidity(), tt = dht.readTemperature();
    if (!isnan(tt) && !isnan(th)) {
      t = tt; h = th;
      if (isnan(tPeak) || t > tPeak) tPeak = t;
      if (t >= ABORT_TEMP_C && phase != 2) lockOff("OVER TEMP");
    } else if (phase == 1) {
      lockOff("DHT FAILED");
    }
    Serial.printf("[phase %d] t=%.1f h=%.1f heater=%s\n",
                  phase, t, h, heaterOn ? "ON " : "off");
  }

  uint32_t elapsed = millis() - phaseStart;
  uint32_t secsLeft = 0;

  if (phase == 0) {
    heater(false);
    secsLeft = (COLD_CHECK_MS - elapsed) / 1000;
    if (elapsed >= COLD_CHECK_MS) {
      tStart = t;
      phase = 1;
      phaseStart = millis();
      Serial.printf("\n=== PHASE 2: burst starting at %.1f C ===\n", tStart);
      heater(true);
    }
  } else if (phase == 1) {
    secsLeft = (BURST_MS - elapsed) / 1000;
    if (elapsed >= BURST_MS) {
      heater(false);
      phase = 2;
      Serial.printf("\n=== DONE: start %.1f C, peak %.1f C, rise %.1f C ===\n",
                    tStart, tPeak, tPeak - tStart);
      Serial.println(F("Heater locked OFF. Reset the board to run again.\n"));
    }
  } else {
    heater(false);
  }

  draw(secsLeft);
  delay(200);
}
