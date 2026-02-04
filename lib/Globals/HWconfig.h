/**
 * @file HWconfig.h
 * @brief Hardware pin definitions and configuration
 * @version 260104P
 * @date 2026-01-04
 *
 * This header defines all hardware pin assignments, feature toggles, and
 * fallback values for the ESP32-based system. Includes GPIO definitions
 * for LED, I2S audio, SD card, SPI, and I2C buses. Also defines hardware
 * status bits for graceful degradation, sensor fallback values, location
 * coordinates for sunrise calculations, and test flags for simulating
 * component failures during development.
 *
 * Hardware Presence (v260104+):
 *   *_PRESENT defines control whether hardware is expected to be present.
 *   Set to false for hardware that is physically absent - no init attempts,
 *   no error flashes, no reminders. Status shows '—' instead of '❌'.
 *   Defines: RTC_PRESENT, DISTANCE_SENSOR_PRESENT, LUX_SENSOR_PRESENT, SENSOR3_PRESENT
 *
 * NOTE: This file should not be changed by chatGPT/Copilot/DeepSeek/Claude/LeChat
 */

#ifndef HWCONFIG_H
#define HWCONFIG_H

#include <Arduino.h>
//#include "config_secrets.h"

// ======================= Device Selection =======================
// Device identifiers (for #if comparison)
#define HOUT   1
#define MARMER 2

// KWAL is set via platformio.ini build_flags (-DKWAL=1 for HOUT, -DKWAL=2 for MARMER)
// Default to HOUT if not defined (for IDE parsing)
#ifndef KWAL
  #define KWAL HOUT
#endif

// ======================= FALLBACK TEST ONLY FLAGS =======================
// Uncomment to simulate component failures during boot testing
// #define TEST_FAIL_SD
// #define TEST_FAIL_WIFI
// #define TEST_FAIL_RTC
// #define TEST_FAIL_NTP
// #define TEST_FAIL_DISTANCE_SENSOR
// #define TEST_FAIL_LUX_SENSOR
// #define TEST_FAIL_SENSOR3

// ======================= Communication Settings =======================
#define SERIAL_BAUD       115200
#define I2C_CLOCK_HZ      400000   // 400kHz Fast clock
#define HALT_BLINK_MS     150      // haltBlink() flash interval

// ======================= Debug & Test Flags =======================
// Uncomment to disable color/pattern shifts (for testing without context shifts)
// #define DISABLE_SHIFTS

// ======================= Pin Definitions =======================
#define LED_PIN 2 // Built-in status LED
#define LED_BUILTIN LED_PIN
#define PIN_LED LED_PIN
#define PIN_RGB 4       // LED Data Output
#define PIN_I2S_DOUT 14 // I2S Data
#define PIN_I2S_BCLK 13 // I2S Bit Clock
#define PIN_I2S_LRC 15  // I2S Word Select (LR Clock)
#define PIN_SD_CS 5     // SD Card Chip Select

// SPI Pins (VSPI)
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK 18

#define SPI_HZ 16000000

// I2C Pins (shared by sensors like BH1750)
#define SDA 21
#define SCL 22
#define I2C_SDA 21
#define I2C_SCL 22

// ======================= Static IP Configuration =======================
#define USE_STATIC_IP true // zet op false voor DHCP

// LAN prefix - single source of truth
#define IP_LAN_STR "192.168.2."
// Device IP addresses (last octet)
#define IP_HOUT   189
#define IP_MARMER 188
// ======================= Hardware Presence =======================
// Auto-configured based on IP_KWAL device selection
// MARMER: RTC + LUX, HOUT: no sensors
#if KWAL == MARMER
  #define RTC_PRESENT true
  #define DISTANCE_SENSOR_PRESENT false
  #define LUX_SENSOR_PRESENT true
  #define SENSOR3_PRESENT false
  #define IP_KWAL IP_MARMER
#else // HOUT
  #define RTC_PRESENT true//false
  #define DISTANCE_SENSOR_PRESENT false
  #define LUX_SENSOR_PRESENT false
  #define SENSOR3_PRESENT false
  #define IP_KWAL IP_HOUT
#endif

// I2C addresses
#define VL53L1X_I2C_ADDR 0x29
#define VEML7700_I2C_ADDR 0x10

// ======================= Location (for sunrise calc) =======================
#define LOCATION_LAT 51.45f   // Eindhoven, Netherlands
#define LOCATION_LON 5.47f

// ======================= Sensor Fallback Defaults =======================
// Used when sensor hardware fails or is absent
#define DISTANCE_SENSOR_DUMMY_MM 9999      // VL53L1X: "far away" - no proximity triggers
#define LUX_SENSOR_DUMMY_LUX 0.5f     // Ambient light: medium brightness
#define SENSOR3_DUMMY_TEMP 25.0f   // Board temp: normal operation

// ======================= Time Fallback =======================
#define FALLBACK_MONTH 4           // April
#define FALLBACK_DAY 20            // 20th
#define FALLBACK_HOUR 4            // 04:00
#define FALLBACK_YEAR 2026

//??DS3231 ???
//ambientlight??

// ======================= Hardware Status Bits =======================
// Runtime flags for graceful degradation (set during boot)
// See docs/fallback_policy.md for degradation behavior per flag
#define HW_SD     (1<<0)
#define HW_WIFI   (1<<1)
#define HW_AUDIO  (1<<2)
#define HW_RGB    (1<<3)
#define HW_LUX    (1<<4)
#define HW_DIST   (1<<5)
#define HW_RTC    (1<<6)
#define HW_I2C    (1<<7)
#define HW_ALL_CRITICAL (HW_SD | HW_AUDIO | HW_RGB)

// ======================= LED Configuration =======================
#define NUM_LEDS 160      // Current LED count in final dome
#define LED_TYPE WS2812B  // LED type used
#define LED_RGB_ORDER GRB // Color order
#define MAX_BRIGHTNESS 250
#define MAX_VOLTS 6
#define MAX_MILLIAMPS 1200
#define BRIGHTNESS_FLOOR 15   // Minimum runtime brightness when non-zero

// ======================= Lux/Brightness =======================
// Design principle: LEDs should BLEND with ambient, not illuminate the room.
// Low ambient lux → low brightness (subtle in dark)
// High ambient lux → high brightness (visible in daylight)
// This is NOT a "night light" that gets brighter when dark.
#define LUX_BETA 0.005f       // Sensitivity of lux -> brightness curve
#define LUX_MAX_LUX 800.0f    // Clamp ambient lux readings
#define LUX_MIN_BASE 70       // Minimum base brightness (0-255)
// ======================= Audio Configuration =======================
#define MAX_VOLUME 0.47f // Maximum audio output volume



// Stringify helper
#define _XSTR(x) #x
#define _STR(x) _XSTR(x)

#define WIFI_SSID "keijebijter"
#define WIFI_PASSWORD "Helmondia;55"
#define OTA_PASSWORD "KwalOTA_3732"
#define OTA_URL "http://" IP_LAN_STR "2/firmware.bin"
#define VOICERSS_API_KEY "9889993b45294559968a1c26c59bc1d1"
// Xeno-canto.org - janwilms@gmail.com/Xeno-cant_3732  -  68276ca7acbfa97a6c627795eb6aad00b547fc1e

// IP strings for fromString() parsing
#define STATIC_IP_STR      IP_LAN_STR _STR(IP_KWAL)
#define STATIC_GATEWAY_STR IP_LAN_STR "254"
#define STATIC_SUBNET_STR  "255.255.255.0"
#define STATIC_DNS_STR     "8.8.8.8"

#endif // HWCONFIG_H
