#pragma once
// ============ microaqila control_v5 configuration ============
// Board: NodeMCU ESP8266 (ESP-12F), chip 0x006BE263.
// Pin names are the board silkscreen labels; GPIO in comments.

#define SERIAL_BAUD       115200

// --- I2C (SSD1306 OLED, addr measured 0x3C, 128x64) ---
#define PIN_I2C_SDA       D2      // GPIO4
#define PIN_I2C_SCL       D1      // GPIO5
#define OLED_ADDR         0x3C
#define OLED_W            128
#define OLED_H            64
#define OLED_RESET_PIN    -1

// --- DHT22 ---
#define PIN_DHT           D5      // GPIO14
#define DHT_TYPE          DHT22

// --- Relays (LOW-trigger CONFIRMED 2026-08-26) ---
// Screw map MEASURED 2026-08-27: K2 = screws 1+2 (fan),
// K1 = screws 2+3 (heater). Middle screw is the bridging contact.
#define PIN_RELAY_HEAT    D6      // GPIO12 -> IN1 -> K1 -> heater
#define PIN_RELAY_FAN     D7      // GPIO13 -> IN2 -> K2 -> 12V fan
#define RELAY_ACTIVE_LOW  1

// --- Control: two thresholds, no band ---
// Defaults chosen for this room (ambient ~31 C) so ambient sits in the
// do-nothing zone. Both are editable from the web page and saved to EEPROM.
#define DEFAULT_MIN_C     30.0    // below this -> heater
#define DEFAULT_MAX_C     33.0    // above this -> fan
#define CTRL_PERIOD_MS    2500    // DHT22 minimum sampling interval is 2 s

// --- Safety (hardware can overheat; these stay) ---
#define ABORT_TEMP_C      45.0    // hard ceiling: everything off above this
#define HEATER_MAX_ON_MS  180000  // 3 min max continuous heat, then forced rest
#define HEATER_REST_MS    60000   // forced rest length after a max-on timeout
#define DHT_FAIL_LIMIT    12      // ~30 s of failed reads -> blind, so all off

// --- Wi-Fi AP ---
#define AP_SSID           "microaqila"
#define AP_PASS           "aqila1234"
