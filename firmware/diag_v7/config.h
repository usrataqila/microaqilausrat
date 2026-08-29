#pragma once
// ================= microaqila board configuration (diag_v7) =================
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

// --- 2ch relay module (SRD-05VDC-SL-C coils, PC817 optos) ---
// [M] 2026-08-25 close-up photos: J1 = RY-VCC | VCC | GND  (jumper shipped on
// RY-VCC+VCC), J2 = GND | IN1 | IN2 | VCC.
// Split-supply wiring: jumper REMOVED, J1 RY-VCC <- VU (5V coils),
// J1 GND <- G, J2 VCC <- 3V (opto side at 3.3V), J2 GND <- G,
// IN1 <- D6, IN2 <- D7.
#define PIN_RELAY_K1      D6    // GPIO12 -> J2 IN1 (heater channel, wired LAST)
#define PIN_RELAY_K2      D7    // GPIO13 -> J2 IN2 (fan channel)
// [M] LOW-trigger CONFIRMED 2026-08-26: LEDs dark through the boot settle
// window, no click at plug-in. Do not change.
#define RELAY_ACTIVE_LOW  1

// --- Terminal-identification phase length ---
#define PHASE_SECS        60    // K2 held steady this long per state

// --- Contact-continuity probe (diag_v7) ---
// The ESP checks the K2 screw contacts itself: D4 drives 3.3 V out, D0 has
// an internal pulldown and listens. If the wired screw pair is closed, D0
// reads HIGH. Both pins are otherwise free; current is microamps.
// D4=GPIO2 is a boot-strap pin but OUTPUT-HIGH after boot is its safe idle
// state; D0=GPIO16 has no strap role and supports INPUT_PULLDOWN_16.
#define PIN_PROBE_SRC     D4    // GPIO2  -> one K2 screw
#define PIN_PROBE_SENSE   D0    // GPIO16 -> another K2 screw
#define PROBE_PHASE_MS    6000  // K2 held off/on this long per phase

// --- control_v1: fan-only cooling thermostat ---
// Ambient measured ~31 C. Thresholds sit just above it so breathing on the
// sensor demonstrates the loop. Hysteresis gap prevents rapid clicking.
#define FAN_ON_C          32.5   // above this -> fan ON
#define FAN_OFF_C         32.0   // below this -> fan off (0.5 C gap)
#define CTRL_PERIOD_MS    2500   // DHT22 minimum sampling is 2 s

// --- Wi-Fi AP + web interface (diag_v7) ---
// ESP hosts its own hotspot; control page lives at http://192.168.4.1
#define AP_SSID           "microaqila"
#define AP_PASS           "aqila1234"   // min 8 chars for WPA2

// --- diag_v7 heater bring-up test ---
#define COLD_CHECK_MS     60000   // heater held OFF: user confirms bank cold
#define BURST_MS          30000   // single supervised ON burst, then OFF for good
#define ABORT_TEMP_C      45.0    // hard abort: sensor above this -> heater off
