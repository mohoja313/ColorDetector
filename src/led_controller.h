/*
 * esp-color-detector — LED Controller
 * ===================================
 * Drives a common-cathode (or common-anode) RGB LED with smooth
 * PWM transitions, gamma correction, and startup animation.
 */

#pragma once

#include <Arduino.h>
#include "config.h"

struct RGB {
  uint8_t r = 0, g = 0, b = 0;
  constexpr RGB() = default;
  constexpr RGB(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}
  constexpr bool operator==(const RGB& o) const { return r==o.r && g==o.g && b==o.b; }
  constexpr bool operator!=(const RGB& o) const { return !(*this == o); }
};

class LedController {
public:
  LedController();
  ~LedController();

  // Initialize PWM channels and pins
  void begin();

  // Call periodically from loop() to animate
  void tick();

  // Set target color (0-255 each). Non-blocking; LED eases toward it.
  void setTarget(uint8_t r, uint8_t g, uint8_t b);
  void setTarget(const RGB& c) { setTarget(c.r, c.g, c.b); }

  // Get current target color
  RGB getTarget() const { return _target; }

  // Get current displayed color (after smoothing)
  RGB getCurrent() const { return _current; }

  // Set global brightness scale (0-100%)
  void setBrightness(uint8_t pct);

  // Trigger startup animation (R→G→B→off)
  void playStartup();

  // Force LED off immediately
  void off();

  // Check if currently animating startup
  bool isAnimating() const { return _animating; }

  // Check if idle timeout has elapsed (no color updates)
  bool isIdle() const;

  // Reset idle timer (call when new color received)
  void resetIdle();

private:
  // Hardware PWM write (handles common-anode inversion)
  void _writePWM(uint8_t r, uint8_t g, uint8_t b);

  // Gamma correction lookup (filled in begin())
  uint8_t _gamma[256];

  // Pin numbers
  const uint8_t _pinR, _pinG, _pinB;

  // PWM state
  RGB _current;          // current displayed color (smoothed)
  RGB _target;           // target color
  uint8_t _brightness = 100;

  // Smoothing
  float _smoothFactor = LED_SMOOTHING_FACTOR;

  // Idle timeout
  unsigned long _lastColorMs = 0;

  // Startup animation
  bool _animating = false;
  uint8_t _animStep = 0;
  unsigned long _animNextMs = 0;
  bool _startupDone = false;
};