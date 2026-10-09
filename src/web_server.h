/*
 * esp-color-detector — Web Server
 * ===============================
 * Serves the gzipped web app, provides REST API (/api/status, /api/color),
 * and WebSocket endpoint (/ws) for real-time color updates.
 * ESP32: HTTPS + WSS; ESP8266: HTTP + WS (on separate port).
 */

#pragma once

#include <Arduino.h>
#include "config.h"
#include "led_controller.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <esp_http_server.h>
  #if FEATURE_HTTPS
    #include <esp_https_server.h>
  #endif
  #define WS_HANDLER_SIG static esp_err_t
  #define HTTP_HANDLER_SIG static esp_err_t
  #define HTTP_REQ httpd_req_t*
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WebServer.h>
  #include <WebSocketsServer.h>
  #define WS_HANDLER_SIG void
  #define HTTP_HANDLER_SIG void
  #define HTTP_REQ ESP8266WebServer&
#endif

class WebServer {
public:
  WebServer(LedController& led);
  ~WebServer();

  // Initialize Wi-Fi AP, DNS, mDNS, HTTP/HTTPS server, WebSocket
  void begin();

  // Call periodically from loop()
  void tick();

  // Get connected client count
  int getClientCount() const { return _clientCount; }

  // Set board name (from config.h)
  void setBoardName(const char* name) { _boardName = name; }

  // Check if HTTPS is active (ESP32 only)
  bool isSecure() const { return _secure; }

  // Get WebSocket port (ESP8266 only, 0 on ESP32 where WSS uses 443)
  uint16_t getWsPort() const { return _wsPort; }

private:
  LedController& _led;
  const char* _boardName = BOARD_NAME;

  // Server state
  bool _secure = false;
  uint16_t _wsPort = 0;
  int _clientCount = 0;
  unsigned long _lastWsCleanup = 0;

  // Wi-Fi / DNS / mDNS
  void _setupWiFi();
  void _setupMDNS();
  void _setupCaptiveDNS();

  // HTTP/HTTPS server - ESP32
  #if defined(ARDUINO_ARCH_ESP32)
    #if FEATURE_HTTPS
      void _setupHTTPSServer();
      void _setupHTTPRedirect();
      httpd_handle_t _httpsServer = nullptr;
      httpd_handle_t _httpServer = nullptr;
    #else
      void _setupHTTPServer();
      httpd_handle_t _httpServer = nullptr;
    #endif
    void _setupWSServer();

    // Request handlers (ESP32 httpd)
    HTTP_HANDLER_SIG _handleRoot(HTTP_REQ req);
    HTTP_HANDLER_SIG _handleStatus(HTTP_REQ req);
    HTTP_HANDLER_SIG _handleColorPost(HTTP_REQ req);
    HTTP_HANDLER_SIG _handleColorGet(HTTP_REQ req);
    HTTP_HANDLER_SIG _handleLedGet(HTTP_REQ req);
    HTTP_HANDLER_SIG _wsHandler(HTTP_REQ req);
    esp_err_t _sendGzipped(HTTP_REQ req, const char* contentType);

  // HTTP/HTTPS server - ESP8266
  #elif defined(ARDUINO_ARCH_ESP8266)
    void _setupHTTPServer();
    void _setupWSServer();
    ESP8266WebServer* _httpServer = nullptr;
    static WebSocketsServer _wsServer;

    // Request handlers (ESP8266WebServer)
    HTTP_HANDLER_SIG _handleRoot();
    HTTP_HANDLER_SIG _handleStatus();
    HTTP_HANDLER_SIG _handleColorPost();
    HTTP_HANDLER_SIG _handleColorGet();
    HTTP_HANDLER_SIG _handleLedGet();
    void _wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
    void _sendGzipped();

  #endif

  // Helpers
  void _cleanupWS();
  void _parseColor(const char* str, uint8_t& r, uint8_t& g, uint8_t& b);

  // Static instance for callbacks
  static WebServer* _instance;
};