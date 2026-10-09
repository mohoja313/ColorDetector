/*
 * esp-color-detector — Configuration
 * ==================================
 * All user-tunable values in one place. Override via build_flags if needed.
 */

#pragma once

/* ──────────────────────────────────────────────────────────────
   Wi-Fi Access Point
   ────────────────────────────────────────────────────────────── */
#define WIFI_SSID           "ESP-ColorDetector"
#define WIFI_PASSWORD       "colors1234"          // min 8 chars
#define WIFI_CHANNEL        6
#define WIFI_MAX_CLIENTS    4
#define AP_IP               IPAddress(192, 168, 4, 1)
#define AP_GATEWAY          IPAddress(192, 168, 4, 1)
#define AP_SUBNET           IPAddress(255, 255, 255, 0)
#define MDNS_HOSTNAME       "colors"              // colors.local

/* ──────────────────────────────────────────────────────────────
   RGB LED — Pin Mapping (per-board defaults, overridable here)
   ────────────────────────────────────────────────────────────── */

// ESP32 / ESP32-S3
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  #ifndef LED_PIN_R
    #define LED_PIN_R       25
  #endif
  #ifndef LED_PIN_G
    #define LED_PIN_G       26
  #endif
  #ifndef LED_PIN_B
    #define LED_PIN_B       27
  #endif
// ESP32-C3
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  #ifndef LED_PIN_R
    #define LED_PIN_R       3
  #endif
  #ifndef LED_PIN_G
    #define LED_PIN_G       4
  #endif
  #ifndef LED_PIN_B
    #define LED_PIN_B       5
  #endif
// ESP8266 / ESP-12E (NodeMCU naming)
#elif defined(ARDUINO_ARCH_ESP8266)
  #ifndef LED_PIN_R
    #define LED_PIN_R       5   // D1
  #endif
  #ifndef LED_PIN_G
    #define LED_PIN_G       4   // D2
  #endif
  #ifndef LED_PIN_B
    #define LED_PIN_B       14  // D5
  #endif
#endif

/* ──────────────────────────────────────────────────────────────
   LED Polarity & PWM
   ────────────────────────────────────────────────────────────── */
// Set to 1 for common-anode LED (invert PWM output)
#ifndef LED_COMMON_ANODE
  #define LED_COMMON_ANODE  0
#endif

// PWM settings
#if defined(ARDUINO_ARCH_ESP32)
  #define LED_PWM_FREQ      5000        // Hz
  #define LED_PWM_RES       8           // bits (0-255)
  #define LED_PWM_MAX       255
#elif defined(ARDUINO_ARCH_ESP8266)
  #define LED_PWM_FREQ      1000        // Hz
  #define LED_PWM_RES       8           // analogWriteRange
  #define LED_PWM_MAX       255
#endif

/* ──────────────────────────────────────────────────────────────
   Color Processing
   ────────────────────────────────────────────────────────────── */
// Enable gamma correction (≈2.2) for perceptual brightness matching
#ifndef LED_GAMMA_CORRECTION
  #define LED_GAMMA_CORRECTION  1
#endif

// LED smoothing (ease factor per tick, ~60 fps, ~120 ms to settle)
// 0.0 = instant, 1.0 = frozen. ~0.08 gives ~120ms at 60Hz.
#ifndef LED_SMOOTHING_FACTOR
  #define LED_SMOOTHING_FACTOR  0.08f
#endif

// LED tick interval (ms) — non-blocking animation loop
#ifndef LED_TICK_MS
  #define LED_TICK_MS  16  // ~60 fps
#endif

// Fade to off after no color update for this many seconds
#ifndef LED_IDLE_TIMEOUT_S
  #define LED_IDLE_TIMEOUT_S  15
#endif

/* ──────────────────────────────────────────────────────────────
   Web Server
   ────────────────────────────────────────────────────────────── */
#define HTTP_PORT         80
#define HTTPS_PORT        443

// WebSocket path
#define WS_PATH           "/ws"

// Max WebSocket frame size we handle (color messages are tiny)
#define WS_MAX_FRAME      64

/* ──────────────────────────────────────────────────────────────
   Board Identification (for /api/status)
   ────────────────────────────────────────────────────────────── */
#if defined(ARDUINO_ARCH_ESP32)
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    #define BOARD_NAME    "ESP32-S3"
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
    #define BOARD_NAME    "ESP32-C3"
  #else
    #define BOARD_NAME    "ESP32"
  #endif
#elif defined(ARDUINO_ARCH_ESP8266)
  #define BOARD_NAME      "ESP8266"
#else
  #define BOARD_NAME      "Unknown"
#endif

/* ──────────────────────────────────────────────────────────────
   Build-time feature flags
   ────────────────────────────────────────────────────────────── */
// Enable HTTPS server (ESP32 family only — requires certs.h)
#if defined(ARDUINO_ARCH_ESP32)
  #define FEATURE_HTTPS   1
#else
  #define FEATURE_HTTPS   0
#endif

// Enable mDNS (works on both ESP32 and ESP8266)
#define FEATURE_MDNS      1

// Enable captive portal DNS (works on both)
#define FEATURE_CAPTIVE   1

// Startup LED animation
#define FEATURE_STARTUP_ANIMATION  1

/* ──────────────────────────────────────────────────────────────
   Debug / Serial
   ────────────────────────────────────────────────────────────── */
#define SERIAL_BAUD       115200
#ifndef DEBUG_VERBOSE
  #define DEBUG_VERBOSE   0
#endif