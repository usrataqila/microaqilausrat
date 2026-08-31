#pragma once
// =====================================================================
//  microaqila / Project G7 - control_v5 configuration
//  Board: NodeMCU ESP8266 (ESP-12F), chip 0x006BE263
//
//  Everything that might need changing lives in this one file, so the
//  main program never has a bare number buried in it. Pin names are the
//  labels printed on the board; the GPIO number is in the comment.
// =====================================================================

#define SERIAL_BAUD       115200
// Speed of the USB serial link. The Serial Monitor must be set to the
// same number or the text comes out as garbage.


// --------------------------- DISPLAY ---------------------------------
#define PIN_I2C_SDA       D2      // GPIO4  - data line
#define PIN_I2C_SCL       D1      // GPIO5  - clock line
#define OLED_ADDR         0x3C
#define OLED_W            128
#define OLED_H            64
#define OLED_RESET_PIN    -1      // -1 = this module has no reset pin
// The OLED talks over I2C, a two-wire bus: one clock, one data. Every
// device on the bus has an address; ours answered at 0x3C when we
// scanned it, so that is what we open. The panel is 128x64 pixels.


// --------------------------- SENSOR ----------------------------------
#define PIN_DHT           D5      // GPIO14 - single data wire
#define DHT_TYPE          DHT22
// The DHT22 is not on the I2C bus. It uses its own one-wire protocol on
// a single pin, which is why it gets a pin of its own.


// --------------------------- RELAYS ----------------------------------
#define PIN_RELAY_HEAT    D6      // GPIO12 -> IN1 -> K1 -> heater
#define PIN_RELAY_FAN     D7      // GPIO13 -> IN2 -> K2 -> 12V fan
#define RELAY_ACTIVE_LOW  1       // 1 = pin LOW switches the relay ON
// D6 and D7 were chosen because they have no job during boot. D3, D4 and
// D8 are read at power-up to decide how the chip starts, so a relay on
// those would click at every reset.
//
// RELAY_ACTIVE_LOW = 1 was confirmed by watching the module's LEDs: they
// stayed dark while the pins were HIGH. This module is LOW-trigger.
//
// Screw terminals, measured with the board itself as a continuity tester:
//   K2 (fan)    = screws 1 + 2
//   K1 (heater) = screws 2 + 3
//   the middle screw is COM on both channels
// Both loads sit on NO (normally open) contacts, so nothing is powered
// unless the ESP actively asks for it.


// ------------------------- CONTROL LIMITS -----------------------------
#define DEFAULT_MIN_C     30.0    // below this -> heater ON
#define DEFAULT_MAX_C     33.0    // above this -> fan ON
#define CTRL_PERIOD_MS    2500    // how often the loop reads and decides
// Two thresholds with a do-nothing zone between them. These are only the
// starting values - both are editable from the web page and saved to
// EEPROM, so they survive a reboot.
//
// Defaults suit this room: ambient is about 31 C, which sits inside the
// 30-33 zone, so the system idles instead of fighting the room.
//
// 2500 ms because the DHT22 cannot be read faster than once every 2 s.


// ----------------------- SETTABLE RANGE -------------------------------
#define USER_MIN_LIMIT_C  15.0
#define USER_MAX_LIMIT_C  38.0
// Guard rails on what the web page will accept. The upper limit is kept
// below ABORT_TEMP_C on purpose: if someone could set the fan threshold
// above the abort ceiling, the safety cut-out would fire before the fan
// ever switched on, which would look like a fault.


// ---------------------------- SAFETY ----------------------------------
#define ABORT_TEMP_C      40.0
// Hard ceiling. Above this everything switches off, no matter what the
// thermostat wants. This is the last line the software can draw.

#define HEATER_MAX_ON_MS  180000  // 3 minutes
#define HEATER_REST_MS    60000   // 1 minute
// The heater is never allowed to run continuously for more than three
// minutes. After that it is forced to rest for a minute before it can
// come back. This catches a stuck demand - a sensor reading low, or a
// heater too weak to ever reach the target - which would otherwise leave
// the heater on indefinitely.

#define DHT_FAIL_LIMIT    12
// Twelve failed reads in a row (about 30 s) means the sensor is gone and
// we are blind. Everything switches off, because heating something we
// cannot measure is exactly the situation to avoid. One good reading
// clears the counter - a single glitch is not enough to trip it.
//
// Beyond all of the above, the hardware has its own guards that work even
// if the ESP crashes: a 3 A fuse, a 105 C thermal cut-out in series with
// the heater, and relays that open on reset or power loss.


// ---------------------------- WI-FI -----------------------------------
#define AP_SSID           "micro1"
#define AP_PASS           "micro1234"
// The ESP creates its own hotspot rather than joining one, so the system
// needs no router and no internet. Password must be 8 characters or more
// or WPA2 refuses to start the access point.
