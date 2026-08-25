// diag_v1 - board bring-up diagnostic for microaqila
// Needs NOTHING wired: prints chip health once, then blinks the onboard
// LED and scans the I2C bus forever. When the OLED is wired correctly
// it will appear in the scan as 0x3C without reflashing.

#include <Wire.h>
#include "config.h"

uint32_t loopCount = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);            // off (active-low)
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial.println();
  Serial.println(F("=================================="));
  Serial.println(F("  microaqila diag_v1  (ESP8266)"));
  Serial.println(F("=================================="));
  Serial.printf("Chip ID:      0x%08X\n", ESP.getChipId());
  Serial.printf("Core ver:     %s\n", ESP.getCoreVersion().c_str());
  Serial.printf("SDK ver:      %s\n", ESP.getSdkVersion());
  Serial.printf("CPU freq:     %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size:   %u KB (chip reports %u KB)\n",
                ESP.getFlashChipSize() / 1024, ESP.getFlashChipRealSize() / 1024);
  Serial.printf("Free heap:    %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Reset reason: %s\n", ESP.getResetReason().c_str());
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("I2C on SDA=D2(GPIO4) SCL=D1(GPIO5), scanning every %d ms\n\n",
                DIAG_LOOP_MS);
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);             // blink = firmware alive
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.printf("[%6lu] I2C: ", (unsigned long)loopCount);
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) { Serial.printf("0x%02X ", addr); found++; }
  }
  if (!found) Serial.print("no devices (expected - nothing wired)");
  Serial.printf(" | heap=%u\n", ESP.getFreeHeap());
  loopCount++;
  delay(DIAG_LOOP_MS - 100);
}
