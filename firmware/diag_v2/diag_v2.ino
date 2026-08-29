// diag_v2 - OLED render test for microaqila.
// An I2C ACK only proves the controller answers; it does not prove the glass
// lights up. This cycles three test screens so every pixel row is exercised,
// and reports over serial whether init succeeded and whether the module is
// still ACKing each frame (so a mid-run dropout shows up as data, not a guess).

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, OLED_RESET_PIN);

uint32_t frame = 0;
bool oledOk = false;

bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);            // off (active-low)
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F("  microaqila diag_v2  (OLED test)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Free heap:    %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("Probe 0x%02X:   %s\n", OLED_ADDR,
                probe(OLED_ADDR) ? "ACK" : "NO ACK");

  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial.printf("SSD1306 init: %s (%dx%d)\n",
                oledOk ? "OK" : "FAILED", OLED_W, OLED_H);
  Serial.printf("Heap after:   %u bytes\n", ESP.getFreeHeap());
  if (!oledOk) {
    Serial.println(F("Buffer alloc or init failed - stopping here."));
    return;
  }
  Serial.println(F("\nCycling: 1=text  2=all-pixels-on  3=checkerboard+border\n"));
}

void screenText() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(F("microaqila"));
  display.setTextSize(1);
  display.printf("OLED OK @ 0x%02X\n", OLED_ADDR);
  display.printf("%dx%d  ESP8266\n", OLED_W, OLED_H);
  display.printf("frame %lu\n", (unsigned long)frame);
  display.printf("heap  %u\n", ESP.getFreeHeap());
  display.drawRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);
  display.display();
}

void screenAllOn() {                          // every pixel lit: finds dead rows
  display.clearDisplay();
  display.fillRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);
  display.display();
}

void screenCheck() {                          // 8px checkerboard + edge border
  display.clearDisplay();
  for (int y = 0; y < OLED_H; y += 8)
    for (int x = 0; x < OLED_W; x += 8)
      if (((x / 8) + (y / 8)) % 2 == 0)
        display.fillRect(x, y, 8, 8, SSD1306_WHITE);
  display.drawRect(0, 0, OLED_W, OLED_H, SSD1306_WHITE);
  display.display();
}

void loop() {
  if (!oledOk) { delay(1000); return; }

  digitalWrite(LED_BUILTIN, LOW);              // blink = firmware alive
  delay(80);
  digitalWrite(LED_BUILTIN, HIGH);

  const uint8_t which = frame % 3;
  switch (which) {
    case 0: screenText();  break;
    case 1: screenAllOn(); break;
    case 2: screenCheck(); break;
  }
  Serial.printf("[%6lu] screen %d drawn | 0x%02X %s | heap=%u\n",
                (unsigned long)frame, which + 1, OLED_ADDR,
                probe(OLED_ADDR) ? "ACK" : "GONE", ESP.getFreeHeap());
  frame++;
  delay(DIAG_FRAME_MS - 80);
}
