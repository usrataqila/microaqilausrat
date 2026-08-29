#pragma once
// ================= microaqila board configuration (diag_v3) =================
// Board: NodeMCU ESP8266 (ESP-12F). Chip ID MEASURED 0x006BE263.
// "Dx" names are the board silkscreen labels; GPIO numbers in comments.

#define SERIAL_BAUD       115200

// --- I2C bus ---
#define PIN_I2C_SDA       D2    // GPIO4  -> OLED SDA
#define PIN_I2C_SCL       D1    // GPIO5  -> OLED SCL

// --- SSD1306 OLED ---
// [M] 2026-08-25: bus scan reports the module ACKing at 0x3C.
#define OLED_ADDR         0x3C
// [G] 0.96" JMD0.96D-1 modules are almost always 128x64. If the panel is
// really 128x32 the test screens will look squashed/doubled - verify by eye.
#define OLED_W            128
#define OLED_H            64
#define OLED_RESET_PIN    -1    // 4-pin module: no reset line

// --- Peripherals (planned; wired later, one at a time) ---
#define PIN_DHT           D5    // GPIO14 - DHT22 data
#define PIN_RELAY_HEATER  D6    // GPIO12 - relay IN1
#define PIN_RELAY_FAN     D7    // GPIO13 - relay IN2

// --- Diagnostic timing ---
#define DIAG_FRAME_MS     3000  // how long each test screen is held

// --- DHT22 (AM2302) ---
#define DHT_TYPE          DHT22  // we have DHT22, not the DHT11 in the proposal
#define DHT_READ_MS       2500   // datasheet minimum sampling interval is 2 s
#define DHT_RETRIES       3      // DHT22 NAKs occasionally; retry before failing
