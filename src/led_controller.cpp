/*
 * esp-color-detector — LED Controller Implementation
 * ==================================================
 */

#include "led_controller.h"
#include "config.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <driver/ledc.h>
#endif

LedController::LedController()
  : _pinR(LED_PIN_R), _pinG(LED_PIN_G), _pinB(LED_PIN_B) {}

LedController::~LedController() {
  off();
}

void LedController::begin() {
  // Build gamma correction table (≈2.2)
  for (int i = 0; i < 256; i++) {
    float v = i / 255.0f;
    v = powf(v, 2.2f);
    _gamma[i] = (uint8_t)(v * 255.0f + 0.5f);
  }

  #if defined(ARDUINO_ARCH_ESP32)
    // ESP32: LEDC (LED Control) peripheral
    // Core 2.x uses ledcSetup/ledcAttachPin, core 3.x uses ledcAttach
    // Use 3 channels (0, 1, 2) for R, G, B
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcAttach(_pinR, LED_PWM_FREQ, LED_PWM_RES);
      ledcAttach(_pinG, LED_PWM_FREQ, LED_PWM_RES);
      ledcAttach(_pinB, LED_PWM_FREQ, LED_PWM_RES);
    #else
      ledcSetup(0, LED_PWM_FREQ, LED_PWM_RES);
      ledcSetup(1, LED_PWM_FREQ, LED_PWM_RES);
      ledcSetup(2, LED_PWM_FREQ, LED_PWM_RES);
      ledcAttachPin(_pinR, 0);
      ledcAttachPin(_pinG, 1);
      ledcAttachPin(_pinB, 2);
    #endif

  #elif defined(ARDUINO_ARCH_ESP8266)
    // ESP8266: analogWrite
    analogWriteFreq(LED_PWM_FREQ);
    analogWriteRange(LED_PWM_MAX);
    pinMode(_pinR, OUTPUT);
    pinMode(_pinG, OUTPUT);
    pinMode(_pinB, OUTPUT);
  #endif

  // Start with LED off
  _writePWM(0, 0, 0);
  _current = RGB(0, 0, 0);
  _target = RGB(0, 0, 0);

  #if FEATURE_STARTUP_ANIMATION
    playStartup();
  #endif

  _lastColorMs = millis();
}

void LedController::tick() {
  unsigned long now = millis();

  // --- Startup animation ---
  if (_animating) {
    if (now >= _animNextMs) {
      _animStep++;
      switch (_animStep) {
        case 1: _writePWM(255, 0, 0); _animNextMs = now + 200; break;   // Red
        case 2: _writePWM(0, 255, 0); _animNextMs = now + 200; break;   // Green
        case 3: _writePWM(0, 0, 255); _animNextMs = now + 200; break;   // Blue
        case 4: _writePWM(0, 0, 0);   _animNextMs = now + 100; break;   // Off
        default:
          _animating = false;
          _startupDone = true;
          _current = RGB(0, 0, 0);
          _target = RGB(0, 0, 0);
          break;
      }
    }
    return;
  }

  // --- Smooth easing toward target ---
  if (_current != _target) {
    float inv = 1.0f - _smoothFactor;
    uint8_t nr = (uint8_t)(_current.r * inv + _target.r * _smoothFactor + 0.5f);
    uint8_t ng = (uint8_t)(_current.g * inv + _target.g * _smoothFactor + 0.5f);
    uint8_t nb = (uint8_t)(_current.b * inv + _target.b * _smoothFactor + 0.5f);

    // If very close, snap to target to avoid endless tiny steps
    if (abs((int)nr - (int)_target.r) <= 1 &&
        abs((int)ng - (int)_target.g) <= 1 &&
        abs((int)nb - (int)_target.b) <= 1) {
      _current = _target;
    } else {
      _current.r = nr;
      _current.g = ng;
      _current.b = nb;
    }
    _writePWM(_current.r, _current.g, _current.b);
  }

  // --- Idle timeout: fade to off if no color updates ---
  if (_target != RGB(0, 0, 0) && (now - _lastColorMs) > (LED_IDLE_TIMEOUT_S * 1000UL)) {
    // Fade out gracefully
    _target = RGB(0, 0, 0);
  }
}

void LedController::setTarget(uint8_t r, uint8_t g, uint8_t b) {
  _target.r = r;
  _target.g = g;
  _target.b = b;
  _lastColorMs = millis();
}

void LedController::setBrightness(uint8_t pct) {
  _brightness = constrain(pct, 0, 100);
  // Re-apply current color with new brightness
  _writePWM(_current.r, _current.g, _current.b);
}

void LedController::playStartup() {
  if (_startupDone) return;
  _animating = true;
  _animStep = 0;
  _animNextMs = millis();
}

void LedController::off() {
  _animating = false;
  _target = RGB(0, 0, 0);
  _current = RGB(0, 0, 0);
  _writePWM(0, 0, 0);
}

bool LedController::isIdle() const {
  return (_target == RGB(0, 0, 0)) &&
         (millis() - _lastColorMs) > (LED_IDLE_TIMEOUT_S * 1000UL);
}

void LedController::resetIdle() {
  _lastColorMs = millis();
}

void LedController::_writePWM(uint8_t r, uint8_t g, uint8_t b) {
  // Apply brightness scaling (0-255 space)
  r = (r * _brightness) / 100;
  g = (g * _brightness) / 100;
  b = (b * _brightness) / 100;

  // Apply gamma correction if enabled
  #if LED_GAMMA_CORRECTION
    r = _gamma[r];
    g = _gamma[g];
    b = _gamma[b];
  #endif

  // Invert for common-anode
  #if LED_COMMON_ANODE
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
  #endif

  #if defined(ARDUINO_ARCH_ESP32)
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(_pinR, r);
      ledcWrite(_pinG, g);
      ledcWrite(_pinB, b);
    #else
      ledcWrite(0, r);
      ledcWrite(1, g);
      ledcWrite(2, b);
    #endif
  #elif defined(ARDUINO_ARCH_ESP8266)
    analogWrite(_pinR, r);
    analogWrite(_pinG, g);
    analogWrite(_pinB, b);
  #endif
}