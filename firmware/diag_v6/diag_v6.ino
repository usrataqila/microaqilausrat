// diag_v6 - relay contact identification WITHOUT a multimeter.
// Two jumpers go from D4 and D0 to two of the K1 screw terminals. D4 drives
// 3.3 V; D0 listens through an internal pulldown. Closed contacts = D0 HIGH.
// K1 alternates 6 s de-energized / 6 s energized and the firmware reports
// whether the wired pair conducts in each state -> names the pair. Dry
// contacts, no 12 V anywhere, probe current is microamps.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);
bool oledOk = false;
uint32_t cycle = 0;
int lastOff = -1, lastOn = -1;   // -1 unknown, 0 open, 1 closed

bool sampleClosed() {
  // majority of 50 reads over 500 ms, after the armature has settled
  delay(500);
  uint8_t hi = 0;
  for (uint8_t i = 0; i < 50; i++) { if (digitalRead(PIN_PROBE_SENSE)) hi++; delay(10); }
  return hi >= 25;
}

const char *verdict() {
  if (lastOff < 0 || lastOn < 0) return "testing...";
  if (!lastOff &&  lastOn) return "PAIR=COM+NO  << USE FOR FAN";
  if ( lastOff && !lastOn) return "PAIR=COM+NC  (wrong one)";
  if (!lastOff && !lastOn) return "no contact in this pair";
  return "ALWAYS closed - check wires";
}

void draw(const char *phase, int state) {
  if (!oledOk) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.printf("%s\n", phase);
  display.printf("%s\n", state < 0 ? "..." : (state ? "CLOSED" : "open"));
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.printf("off:%s on:%s\n",
                 lastOff < 0 ? "?" : (lastOff ? "closed" : "open"),
                 lastOn  < 0 ? "?" : (lastOn  ? "closed" : "open"));
  display.println(verdict());
  display.display();
}

void setup() {
  digitalWrite(PIN_RELAY_K2, RELAY_OFF); pinMode(PIN_RELAY_K2, OUTPUT);
  digitalWrite(PIN_RELAY_K1, RELAY_OFF); pinMode(PIN_RELAY_K1, OUTPUT);
  digitalWrite(PIN_PROBE_SRC, HIGH);     pinMode(PIN_PROBE_SRC, OUTPUT);
  pinMode(PIN_PROBE_SENSE, INPUT_PULLDOWN_16);

  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F("  microaqila diag_v6 (contact ID)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Probe: D4 drives 3.3V, D0 listens (pulldown). K2 stays OFF.\n");
  Serial.printf("K1 alternates %d ms OFF / %d ms ON.\n\n", PROBE_PHASE_MS, PROBE_PHASE_MS);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s\n\n", oledOk ? "OK" : "FAILED");
}

void loop() {
  digitalWrite(PIN_RELAY_K1, RELAY_OFF);
  draw("K1 off", -1);
  lastOff = sampleClosed() ? 1 : 0;
  Serial.printf("[cyc %lu] K1 de-energized: pair %s\n",
                (unsigned long)cycle, lastOff ? "CLOSED" : "open");
  draw("K1 off", lastOff);
  delay(PROBE_PHASE_MS - 1000);

  digitalWrite(PIN_RELAY_K1, RELAY_ON);
  draw("K1 ON", -1);
  lastOn = sampleClosed() ? 1 : 0;
  Serial.printf("[cyc %lu] K1 energized:    pair %s   => %s\n\n",
                (unsigned long)cycle, lastOn ? "CLOSED" : "open", verdict());
  draw("K1 ON", lastOn);
  delay(PROBE_PHASE_MS - 1000);
  cycle++;
}
