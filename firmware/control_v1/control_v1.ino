// control_v1 - FIRST CLOSED LOOP for microaqila: fan-only cooling thermostat.
// Wired today: DHT22 (sense), OLED (display), K2 relay -> 12V fan (act).
// Logic: temp > FAN_ON_C -> fan on; temp < FAN_OFF_C -> fan off; in between
// keep last state (hysteresis). K1 (heater, not wired) is held OFF always.
// Counts relay switch events - the metric the capstone report compares.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
DHT dht(PIN_DHT, DHT_TYPE);

bool fanOn = false, oledOk = false;
uint32_t switches = 0, ticks = 0, dhtFails = 0;
float t = NAN, h = NAN;

void draw() {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  if (isnan(t)) {
    display.setTextSize(2);
    display.setCursor(0, 8);
    display.println(F("DHT FAIL"));
  } else {
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.printf("%.1f", t);
    display.setTextSize(1);
    display.print(F("C"));
    display.setTextSize(2);
    display.setCursor(0, 30);
    display.printf("FAN %s", fanOn ? "ON" : "--");
  }
  display.setTextSize(1);
  display.setCursor(0, 54);
  display.printf("h%.0f%% on>%.1f off<%.1f sw%lu", h, FAN_ON_C, FAN_OFF_C,
                 (unsigned long)switches);
  display.display();
}

void setFan(bool on) {
  if (on == fanOn) return;
  fanOn = on;
  switches++;
  digitalWrite(PIN_RELAY_K2, on ? RELAY_ON : RELAY_OFF);
  Serial.printf(">>> FAN %s at %.1f C (switch #%lu)\n",
                on ? "ON" : "OFF", t, (unsigned long)switches);
}

void setup() {
  digitalWrite(PIN_RELAY_K1, RELAY_OFF); pinMode(PIN_RELAY_K1, OUTPUT);
  digitalWrite(PIN_RELAY_K2, RELAY_OFF); pinMode(PIN_RELAY_K2, OUTPUT);
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F(" microaqila control_v1 (fan loop)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
  Serial.printf("Fan ON above %.1f C, OFF below %.1f C. Heater held OFF.\n",
                FAN_ON_C, FAN_OFF_C);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n", oledOk ? "OK" : "FAILED");
  dht.begin();
  Serial.printf("Free heap:    %u bytes\n\n", ESP.getFreeHeap());
}

void loop() {
  float th = dht.readHumidity(), tt = dht.readTemperature();
  if (!isnan(tt) && !isnan(th)) { t = tt; h = th; }
  else dhtFails++;

  if (!isnan(t)) {
    if (t > FAN_ON_C)       setFan(true);
    else if (t < FAN_OFF_C) setFan(false);
    // between the two: keep current state (hysteresis band)
  }
  draw();
  if (ticks % 4 == 0)
    Serial.printf("[%5lu] t=%.1f h=%.1f fan=%s sw=%lu fails=%lu heap=%u\n",
                  (unsigned long)ticks, t, h, fanOn ? "ON " : "off",
                  (unsigned long)switches, (unsigned long)dhtFails,
                  ESP.getFreeHeap());
  ticks++;
  delay(CTRL_PERIOD_MS);
}
