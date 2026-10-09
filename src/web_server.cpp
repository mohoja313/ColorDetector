/*
 * esp-color-detector — Web Server Implementation
 * ==============================================
 */

#include "web_server.h"
#include "web_assets.h"
#include "config.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <DNSServer.h>
  #include <esp_http_server.h>
  #include <esp_https_server.h>
  #include <esp_wifi.h>
  #if FEATURE_MDNS
    #include <ESPmDNS.h>
  #endif
  #include "certs.h"
#elif defined(ARDUINO_ARCH_ESP8266)
  #include <ESP8266WiFi.h>
  #include <DNSServer.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266mDNS.h>
  #include <WebSocketsServer.h>
#endif

// ── Static callback trampolines ─────────────────────────────────
WebServer* WebServer::_instance = nullptr;

#if defined(ARDUINO_ARCH_ESP8266)
WebSocketsServer WebServer::_wsServer(81);
#endif

WebServer::WebServer(LedController& led) : _led(led) {
  _instance = this;
}

WebServer::~WebServer() {
  _instance = nullptr;
}

// ══════════════════════════════════════════════════════════════════
// COMMON: Wi-Fi AP, mDNS, DNS
// ══════════════════════════════════════════════════════════════════

void WebServer::_setupWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, WIFI_MAX_CLIENTS);

  Serial.println();
  Serial.println("═══════════════════════════════════════");
  Serial.print  ("  ESP Color Detector — ");
  Serial.println(_boardName);
  Serial.println("═══════════════════════════════════════");
  Serial.print  ("  SSID:     "); Serial.println(WIFI_SSID);
  Serial.print  ("  Password: "); Serial.println(WIFI_PASSWORD);
  Serial.print  ("  AP IP:    "); Serial.println(WiFi.softAPIP());
  Serial.print  ("  URL:      "); 
  #if defined(ARDUINO_ARCH_ESP32) && FEATURE_HTTPS
    Serial.print("https://"); 
  #else
    Serial.print("http://"); 
  #endif
  Serial.print(WiFi.softAPIP()); Serial.println("/");
  Serial.print  ("  mDNS:     http://"); Serial.print(MDNS_HOSTNAME); Serial.println(".local/");
  Serial.println("═══════════════════════════════════════");
  Serial.println();
}

void WebServer::_setupMDNS() {
  #if FEATURE_MDNS
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      #if defined(ARDUINO_ARCH_ESP32) && FEATURE_HTTPS
        MDNS.addService("https", "tcp", 443);
      #endif
      Serial.printf("[mDNS] Started: %s.local\n", MDNS_HOSTNAME);
    } else {
      Serial.println("[mDNS] Failed to start");
    }
  #endif
}

void WebServer::_parseColor(const char* str, uint8_t& r, uint8_t& g, uint8_t& b) {
  // Format: "r,g,b" or "RRGGBB"
  if (strchr(str, ',')) {
    r = constrain(atoi(str), 0, 255);
    char* p = strchr(str, ',');
    if (p) g = constrain(atoi(p + 1), 0, 255);
    if (p) { p = strchr(p + 1, ','); if (p) b = constrain(atoi(p + 1), 0, 255); }
  } else if (strlen(str) >= 6) {
    r = strtol(str, nullptr, 16);
    g = strtol(str + 2, nullptr, 16);
    b = strtol(str + 4, nullptr, 16);
  }
}

// ══════════════════════════════════════════════════════════════════
// ESP32 IMPLEMENTATION
// ══════════════════════════════════════════════════════════════════
#if defined(ARDUINO_ARCH_ESP32)

void WebServer::_setupHTTPRedirect() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_PORT;
  config.ctrl_port = HTTP_PORT + 100;
  config.max_open_sockets = 4;

  httpd_uri_t redirect = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = [](httpd_req_t* req) -> esp_err_t {
      httpd_resp_set_status(req, "302 Found");
      httpd_resp_set_hdr(req, "Location", "https://192.168.4.1/");
      httpd_resp_send(req, nullptr, 0);
      return ESP_OK;
    },
    .user_ctx = nullptr
  };

  if (httpd_start(&_httpServer, &config) == ESP_OK) {
    httpd_register_uri_handler(_httpServer, &redirect);
    Serial.println("[HTTP] Redirect server on port 80");
  }
}

void WebServer::_setupHTTPSServer() {
  httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
  config.httpd.server_port = HTTPS_PORT;
  config.httpd.ctrl_port = HTTPS_PORT + 100;
  config.httpd.max_open_sockets = 7;
  config.httpd.max_uri_handlers = 16;
  config.httpd.recv_wait_timeout = 10;
  config.httpd.send_wait_timeout = 10;

  config.cacert_pem = (const uint8_t*)SERVER_CERT;
  config.cacert_len = SERVER_CERT_LEN;
  config.prvtkey_pem = (const uint8_t*)SERVER_KEY;
  config.prvtkey_len = SERVER_KEY_LEN;

  if (httpd_ssl_start(&_httpsServer, &config) == ESP_OK) {
    _secure = true;
    Serial.println("[HTTPS] Server started on port 443");

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = _handleRoot, .user_ctx = this };
    httpd_register_uri_handler(_httpsServer, &root);

    httpd_uri_t status = { .uri = "/api/status", .method = HTTP_GET, .handler = _handleStatus, .user_ctx = this };
    httpd_register_uri_handler(_httpsServer, &status);

    httpd_uri_t colorPost = { .uri = "/api/color", .method = HTTP_POST, .handler = _handleColorPost, .user_ctx = this };
    httpd_register_uri_handler(_httpsServer, &colorPost);

    httpd_uri_t colorGet = { .uri = "/api/color", .method = HTTP_GET, .handler = _handleColorGet, .user_ctx = this };
    httpd_register_uri_handler(_httpsServer, &colorGet);

    httpd_uri_t ledGet = { .uri = "/api/led", .method = HTTP_GET, .handler = _handleLedGet, .user_ctx = this };
    httpd_register_uri_handler(_httpsServer, &ledGet);

    httpd_uri_t ws = { .uri = WS_PATH, .method = HTTP_GET, .handler = _wsHandler, .user_ctx = this, .is_websocket = true };
    httpd_register_uri_handler(_httpsServer, &ws);

  } else {
    Serial.println("[HTTPS] Failed to start server");
  }
}

void WebServer::_setupWSServer() {
  // WebSocket is handled by httpd URI handler with is_websocket=true
}

#if !FEATURE_HTTPS
// ESP32 HTTP-only server (when HTTPS disabled)
void WebServer::_setupHTTPServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_PORT;
  config.ctrl_port = HTTP_PORT + 100;
  config.max_open_sockets = 7;
  config.max_uri_handlers = 16;
  config.recv_wait_timeout = 10;
  config.send_wait_timeout = 10;

  if (httpd_start(&_httpServer, &config) == ESP_OK) {
    _secure = false;
    Serial.println("[HTTP] Server started on port 80");

    httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = _handleRoot, .user_ctx = this };
    httpd_register_uri_handler(_httpServer, &root);

    httpd_uri_t status = { .uri = "/api/status", .method = HTTP_GET, .handler = _handleStatus, .user_ctx = this };
    httpd_register_uri_handler(_httpServer, &status);

    httpd_uri_t colorPost = { .uri = "/api/color", .method = HTTP_POST, .handler = _handleColorPost, .user_ctx = this };
    httpd_register_uri_handler(_httpServer, &colorPost);

    httpd_uri_t colorGet = { .uri = "/api/color", .method = HTTP_GET, .handler = _handleColorGet, .user_ctx = this };
    httpd_register_uri_handler(_httpServer, &colorGet);

    httpd_uri_t ledGet = { .uri = "/api/led", .method = HTTP_GET, .handler = _handleLedGet, .user_ctx = this };
    httpd_register_uri_handler(_httpServer, &ledGet);

    httpd_uri_t ws = { .uri = WS_PATH, .method = HTTP_GET, .handler = _wsHandler, .user_ctx = this, .is_websocket = true };
    httpd_register_uri_handler(_httpServer, &ws);
  } else {
    Serial.println("[HTTP] Failed to start server");
  }
}
#endif

esp_err_t WebServer::_handleRoot(httpd_req_t* req) {
  WebServer* self = (WebServer*)req->user_ctx;
  return self->_sendGzipped(req, "text/html");
}

esp_err_t WebServer::_handleStatus(httpd_req_t* req) {
  WebServer* self = (WebServer*)req->user_ctx;

  char json[256];
  snprintf(json, sizeof(json),
    "{"
    "\"board\":\"%s\","
    "\"ip\":\"192.168.4.1\","
    "\"clients\":%d,"
    "\"uptime\":%lu,"
    "\"pins\":{\"r\":%d,\"g\":%d,\"b\":%d},"
    "\"secure\":%s,"
    "\"ledPolarity\":\"%s\""
    "}",
    self->_boardName,
    self->_clientCount,
    millis() / 1000,
    LED_PIN_R, LED_PIN_G, LED_PIN_B,
    self->_secure ? "true" : "false",
    LED_COMMON_ANODE ? "common-anode" : "common-cathode"
  );

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_send(req, json, strlen(json));
  return ESP_OK;
}

esp_err_t WebServer::_handleColorPost(httpd_req_t* req) {
  WebServer* self = (WebServer*)req->user_ctx;

  char buf[32];
  int len = req->content_len;
  if (len > 0 && len < (int)sizeof(buf)) {
    int ret = httpd_req_recv(req, buf, len);
    if (ret > 0) {
      buf[ret] = '\0';
      uint8_t r, g, b;
      self->_parseColor(buf, r, g, b);
      self->_led.setTarget(r, g, b);
      self->_led.resetIdle();
      httpd_resp_send(req, "OK", 2);
      return ESP_OK;
    }
  }
  httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid color");
  return ESP_FAIL;
}

esp_err_t WebServer::_handleColorGet(httpd_req_t* req) {
  WebServer* self = (WebServer*)req->user_ctx;

  char* buf;
  size_t len = httpd_req_get_url_query_len(req) + 1;
  if (len > 1) {
    buf = (char*)malloc(len);
    if (httpd_req_get_url_query_str(req, buf, len) == ESP_OK) {
      uint8_t r = 0, g = 0, b = 0;
      char* p = strstr(buf, "r="); if (p) r = atoi(p + 2);
      p = strstr(buf, "g="); if (p) g = atoi(p + 2);
      p = strstr(buf, "b="); if (p) b = atoi(p + 2);
      self->_led.setTarget(r, g, b);
      self->_led.resetIdle();
      httpd_resp_send(req, "OK", 2);
      free(buf);
      return ESP_OK;
    }
    free(buf);
  }
  httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing params");
  return ESP_FAIL;
}

esp_err_t WebServer::_handleLedGet(httpd_req_t* req) {
  WebServer* self = (WebServer*)req->user_ctx;

  char* buf;
  size_t len = httpd_req_get_url_query_len(req) + 1;
  if (len > 1) {
    buf = (char*)malloc(len);
    if (httpd_req_get_url_query_str(req, buf, len) == ESP_OK) {
      char* p = strstr(buf, "hex=");
      if (p) {
        p += 4;
        if (strlen(p) >= 6) {
          uint8_t r = strtol(p, nullptr, 16);
          uint8_t g = strtol(p + 2, nullptr, 16);
          uint8_t b = strtol(p + 4, nullptr, 16);
          self->_led.setTarget(r, g, b);
          self->_led.resetIdle();
          httpd_resp_send(req, "OK", 2);
          free(buf);
          return ESP_OK;
        }
      }
      free(buf);
    }
  }
  httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid hex");
  return ESP_FAIL;
}

esp_err_t WebServer::_wsHandler(httpd_req_t* req) {
  WebServer* self = (WebServer*)req->user_ctx;

  if (req->method == HTTP_GET) {
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len > 0) {
      char* buf = (char*)malloc(ws_pkt.len + 1);
      ws_pkt.payload = (uint8_t*)buf;
      ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
      if (ret == ESP_OK) {
        buf[ws_pkt.len] = '\0';
        if (buf[0] == 'c' && buf[1] == ':' && ws_pkt.len >= 8) {
          uint8_t r, g, b;
          self->_parseColor(buf + 2, r, g, b);
          self->_led.setTarget(r, g, b);
          self->_led.resetIdle();
          httpd_ws_frame_t ack = { .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t*)"a", .len = 1 };
          httpd_ws_send_frame(req, &ack);
        } else if (buf[0] == 'b' && buf[1] == ':') {
          uint8_t bright = atoi(buf + 2);
          self->_led.setBrightness(bright);
        } else if (buf[0] == 'h') {
          httpd_ws_frame_t ack = { .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t*)"a", .len = 1 };
          httpd_ws_send_frame(req, &ack);
        }
      }
      free(buf);
    }
    return ESP_OK;
  }
  return ESP_FAIL;
}

esp_err_t WebServer::_sendGzipped(httpd_req_t* req, const char* contentType) {
  httpd_resp_set_type(req, contentType);
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
  return httpd_resp_send(req, (const char*)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
}

void WebServer::_cleanupWS() {
  // ESP32 httpd handles this automatically
}

// ══════════════════════════════════════════════════════════════════
// ESP8266 IMPLEMENTATION
// ══════════════════════════════════════════════════════════════════
#elif defined(ARDUINO_ARCH_ESP8266)

void WebServer::_setupHTTPServer() {
  _httpServer = new ESP8266WebServer(HTTP_PORT);
  _httpServer->on("/", [this]() { _handleRoot(); });
  _httpServer->on("/api/status", [this]() { _handleStatus(); });
  _httpServer->on("/api/color", HTTP_POST, [this]() { _handleColorPost(); });
  _httpServer->on("/api/color", HTTP_GET, [this]() { _handleColorGet(); });
  _httpServer->on("/api/led", HTTP_GET, [this]() { _handleLedGet(); });
  _httpServer->begin();
  _secure = false;
  Serial.println("[HTTP] Server started on port 80");
}

void WebServer::_setupWSServer() {
  _wsPort = 81;
  _wsServer.begin();
  _wsServer.onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
    if (_instance) _instance->_wsEvent(num, type, payload, len);
  });
  Serial.println("[WS] Server started on port 81");
}

void WebServer::_handleRoot() {
  _sendGzipped();
}

void WebServer::_handleStatus() {
  char json[256];
  snprintf(json, sizeof(json),
    "{"
    "\"board\":\"%s\","
    "\"ip\":\"192.168.4.1\","
    "\"clients\":%d,"
    "\"uptime\":%lu,"
    "\"pins\":{\"r\":%d,\"g\":%d,\"b\":%d},"
    "\"secure\":%s,"
    "\"ledPolarity\":\"%s\""
    "}",
    _boardName,
    _clientCount,
    millis() / 1000,
    LED_PIN_R, LED_PIN_G, LED_PIN_B,
    _secure ? "true" : "false",
    LED_COMMON_ANODE ? "common-anode" : "common-cathode"
  );

  _httpServer->send(200, "application/json", json);
}

void WebServer::_handleColorPost() {
  String body = _httpServer->arg("plain");
  if (body.length() > 0 && body.length() < 32) {
    uint8_t r, g, b;
    _parseColor(body.c_str(), r, g, b);
    _led.setTarget(r, g, b);
    _led.resetIdle();
    _httpServer->send(200, "text/plain", "OK");
    return;
  }
  _httpServer->send(400, "text/plain", "Invalid color");
}

void WebServer::_handleColorGet() {
  uint8_t r = 0, g = 0, b = 0;
  if (_httpServer->hasArg("r")) r = _httpServer->arg("r").toInt();
  if (_httpServer->hasArg("g")) g = _httpServer->arg("g").toInt();
  if (_httpServer->hasArg("b")) b = _httpServer->arg("b").toInt();
  _led.setTarget(r, g, b);
  _led.resetIdle();
  _httpServer->send(200, "text/plain", "OK");
}

void WebServer::_handleLedGet() {
  if (_httpServer->hasArg("hex")) {
    String hex = _httpServer->arg("hex");
    if (hex.length() >= 6) {
      uint8_t r = strtol(hex.c_str(), nullptr, 16);
      uint8_t g = strtol(hex.c_str() + 2, nullptr, 16);
      uint8_t b = strtol(hex.c_str() + 4, nullptr, 16);
      _led.setTarget(r, g, b);
      _led.resetIdle();
      _httpServer->send(200, "text/plain", "OK");
      return;
    }
  }
  _httpServer->send(400, "text/plain", "Invalid hex");
}

void WebServer::_wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type == WStype_CONNECTED) {
    _clientCount++;
    _wsServer.sendTXT(num, "a");
  } else if (type == WStype_DISCONNECTED) {
    _clientCount = max(0, _clientCount - 1);
  } else if (type == WStype_TEXT) {
    char* msg = (char*)payload;
    if (msg[0] == 'c' && msg[1] == ':' && len >= 8) {
      uint8_t r, g, b;
      _parseColor(msg + 2, r, g, b);
      _led.setTarget(r, g, b);
      _led.resetIdle();
      _wsServer.sendTXT(num, "a");
    } else if (msg[0] == 'b' && msg[1] == ':') {
      uint8_t bright = atoi(msg + 2);
      _led.setBrightness(bright);
    }
  }
}

void WebServer::_sendGzipped() {
  _httpServer->sendHeader("Content-Encoding", "gzip");
  _httpServer->sendHeader("Cache-Control", "public, max-age=31536000");
  _httpServer->send_P(200, "text/html", (const char*)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
}

void WebServer::_cleanupWS() {
  // Handled by WebSocketsServer library
}

#endif

// ══════════════════════════════════════════════════════════════════
// PUBLIC API
// ══════════════════════════════════════════════════════════════════

void WebServer::begin() {
  _setupWiFi();
  _setupMDNS();

  #if defined(ARDUINO_ARCH_ESP32)
    #if FEATURE_HTTPS
      _setupHTTPRedirect();
      _setupHTTPSServer();
    #else
      _setupHTTPServer();
    #endif
    _setupWSServer();
  #elif defined(ARDUINO_ARCH_ESP8266)
    _setupHTTPServer();
    _setupWSServer();
  #endif

  // Captive portal DNS
  #if FEATURE_CAPTIVE
    static DNSServer dns;
    dns.start(53, "*", AP_IP);
    Serial.println("[DNS] Captive portal on port 53");
  #endif
}

void WebServer::tick() {
  #if defined(ARDUINO_ARCH_ESP32)
    // ESP32 httpd handles async - just need to process DNS
  #elif defined(ARDUINO_ARCH_ESP8266)
    if (_httpServer) _httpServer->handleClient();
    _wsServer.loop();
  #endif

  // Clean up stale WS connections every 30s
  unsigned long now = millis();
  if (now - _lastWsCleanup > 30000) {
    _lastWsCleanup = now;
    _cleanupWS();
  }
}