#pragma once
// ================= microaqila board configuration =================
// Board: NodeMCU ESP8266 (ESP-12F). Chip identity MEASURED with esptool
// on 2026-08-25: ESP8266EX  (bag was mislabeled "ESP32 DevKit V1").
//
// All pin assignments and tunables live in this one file.
// "Dx" names are the board silkscreen labels; GPIO numbers in comments.
//
// Pin choices deliberately avoid ESP8266 boot-strapping pins
// (D3=GPIO0, D4=GPIO2, D8=GPIO15) which glitch during boot and would
// pulse the relays on every reset.

#define SERIAL_BAUD       115200

// --- I2C bus (SSD1306 OLED, addr 0x3C expected) ---
#define PIN_I2C_SDA       D2    // GPIO4
#define PIN_I2C_SCL       D1    // GPIO5

// --- Peripherals (planned; wired later, one at a time) ---
#define PIN_DHT           D5    // GPIO14 - DHT22 data
#define PIN_RELAY_HEATER  D6    // GPIO12 - relay IN1
#define PIN_RELAY_FAN     D7    // GPIO13 - relay IN2

// --- Diagnostic timing ---
#define DIAG_LOOP_MS      2000
