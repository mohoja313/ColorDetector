/*
 * esp-color-detector — Main Application
 * =====================================
 * Entry point for the firmware. Initializes hardware, Wi-Fi AP,
 * web server, and runs the main loop.
 */

#include <Arduino.h>
#include "config.h"
#include "led_controller.h"
#include "web_server.h"

// Global instances
LedController led;
WebServer server(led);

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 2000) { delay(10); }

  Serial.println();
  Serial.println("═══════════════════════════════════════");
  Serial.print  ("  ESP Color Detector — ");
  Serial.println(BOARD_NAME);
  Serial.println("═══════════════════════════════════════");
  #if defined(ARDUINO_ARCH_ESP32)
    Serial.print  ("  Core:     "); Serial.println(ESP.getChipModel());
    Serial.print  ("  Rev:      "); Serial.println(ESP.getChipRevision());
    Serial.print  ("  Freq:     "); Serial.print(ESP.getCpuFreqMHz()); Serial.println(" MHz");
    Serial.print  ("  Flash:    "); Serial.print(ESP.getFlashChipSize() / 1024); Serial.println(" KB");
    Serial.print  ("  SDK:      "); Serial.println(ESP.getSdkVersion());
    Serial.print  ("  Core ver: "); Serial.println(ESP_ARDUINO_VERSION);
  #elif defined(ARDUINO_ARCH_ESP8266)
    Serial.print  ("  Chip ID:  "); Serial.println(ESP.getChipId(), HEX);
    Serial.print  ("  Flash ID: "); Serial.println(ESP.getFlashChipId(), HEX);
    Serial.print  ("  Freq:     "); Serial.print(ESP.getCpuFreqMHz()); Serial.println(" MHz");
    Serial.print  ("  Flash:    "); Serial.print(ESP.getFlashChipSize() / 1024); Serial.println(" KB");
    Serial.print  ("  SDK:      "); Serial.println(ESP.getSdkVersion());
  #endif
  Serial.println("═══════════════════════════════════════");

  // Initialize LED controller
  led.begin();

  // Initialize web server (Wi-Fi AP, DNS, HTTP/HTTPS, WebSocket)
  server.begin();
  server.setBoardName(BOARD_NAME);

  Serial.println("[Main] Setup complete — ready for connections");
  Serial.println();
}

void loop() {
  // Run web server tasks (WebSocket polling, DNS, etc.)
  server.tick();

  // Animate LED (smoothing, startup, idle fade)
  led.tick();

  // Yield to prevent watchdog reset
  #if defined(ARDUINO_ARCH_ESP32)
    vTaskDelay(pdMS_TO_TICKS(2));
  #elif defined(ARDUINO_ARCH_ESP8266)
    yield();
    delay(2);
  #endif
}