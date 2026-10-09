# ESP Color Detector

A production-quality embedded project that turns an ESP32/ESP8266 into a wireless color detection instrument. Point your phone's camera at any color and watch it mirrored in real time on an RGB LED.

![ESP Color Detector](docs/demo.gif)

## Features

- **Real-time color detection** — Runs entirely in the browser using the Camera API and Canvas; zero video streaming to the ESP
- **Works on ESP32, ESP32-S3, ESP32-C3, and ESP8266/ESP-12E** — Single codebase, multiple targets
- **Secure HTTPS + WSS on ESP32** — Self-signed certificate with SAN for `192.168.4.1` and `colors.local`; required for camera access on mobile browsers
- **Graceful fallback on ESP8266** — Photo capture + manual color picker when HTTPS isn't available
- **Perceptually accurate LED output** — Gamma correction (γ≈2.2), smooth easing (~120 ms), 8-bit PWM at 5 kHz
- **Premium mobile-first UI** — Obsidian dark theme, glassmorphism, ambient color glow, 60 fps animations, haptic feedback
- **Self-contained** — All web assets (HTML/CSS/JS) minified, gzipped, and embedded in PROGMEM (~14 KB gzipped)
- **Captive portal + mDNS** — Join the AP, open browser, it just works
- **PlatformIO + Arduino IDE compatible** — Standard `src/main.cpp` layout

## Hardware

| Component | Qty | Notes |
|-----------|-----|-------|
| ESP32 DevKit V1 (or ESP32-S3-DevKitC-1, ESP32-C3-DevKitM-1, ESP-12E/NodeMCU) | 1 | Any supported board |
| Common-cathode RGB LED (5mm or SMD) | 1 | Or common-anode with `LED_COMMON_ANODE=1` |
| 220 Ω resistors (or 330 Ω) | 3 | One per color channel |
| Breadboard + jumper wires | 1 set | |
| USB cable | 1 | For power/flash |

> **Total cost:** ~$8–15 depending on board

## Wiring

See [`docs/wiring.md`](docs/wiring.md) for detailed per-board pinout tables and ASCII diagrams.

### Quick Reference (ESP32 DevKit)

```
ESP32 DevKit          RGB LED (Common Cathode)
─────────────────────────────────────────────────────
GPIO 25  ──[220Ω]──►  R (Red anode)
GPIO 26  ──[220Ω]──►  G (Green anode)
GPIO 27  ──[220Ω]──►  B (Blue anode)
GND  ──────────────►  Common Cathode (longest leg)
```

## Quick Start

### PlatformIO (Recommended)

```bash
# Clone and enter directory
cd esp-color-detector

# Build for ESP32 DevKit
pio run -e esp32dev

# Flash and monitor
pio run -t upload -e esp32dev
pio device monitor
```

### Arduino IDE

1. Open `esp-color-detector` folder in Arduino IDE
2. Select your board: **Tools → Board → ESP32 Arduino → ESP32 Dev Module** (or your board)
3. Install required libraries via Library Manager:
   - `WebSockets` by Markus Sattler (for ESP8266 only)
4. Click **Upload**
5. Open **Serial Monitor** at 115200 baud

## Usage

1. **Power the board** — LED runs startup animation (R → G → B → off)
2. **Connect to Wi-Fi** — On your phone, join:
   - **SSID:** `ESP-ColorDetector`
   - **Password:** `colors1234`
3. **Open the app** — Browser opens automatically (captive portal), or manually navigate to:
   - `https://192.168.4.1` (ESP32 family)
   - `http://192.168.4.1` (ESP8266)
   - `https://colors.local` (mDNS, ESP32 only)
4. **Tap "Enable Camera"** — Grant permission when prompted
5. **Point at colors** — The color card and LED update in real time
6. **Lock a color** — Tap the video or press 🔒 to freeze; tap 📋 to copy HEX

### ESP32 HTTPS Certificate Warning

Mobile browsers require a **secure context** (HTTPS) for camera access. The ESP32 serves a self-signed certificate.

**First visit:** Browser shows "Your connection is not private" / "Warning: Potential Security Risk Ahead"

- **Chrome/Edge:** Click **Advanced → Proceed to 192.168.4.1 (unsafe)**
- **Firefox:** Click **Advanced → Accept the Risk and Continue**
- **Safari (iOS):** Tap **Show Details → Visit this Website**

> This is normal for local self-signed certificates. The connection is still encrypted; only the *identity* isn't verified by a public CA.

**Alternative (Android Chrome only):** Enable `chrome://flags/#unsafely-treat-insecure-origin-as-secure` and add `http://192.168.4.1` — then use plain HTTP without warnings.

### ESP8266 — No Live Camera

The ESP8266 serves plain HTTP. Browsers block `getUserMedia` on insecure origins. The UI automatically falls back to:

1. **📸 Capture Photo** — Opens camera, takes a still, detects center color
2. **🎨 Manual Picker** — Full HSV color picker to push any color to the LED

> For live camera streaming, use an ESP32, ESP32-S3, or ESP32-C3.

## Configuration

All tunable values in [`src/config.h`](src/config.h):

```cpp
// Wi-Fi AP
#define WIFI_SSID           "ESP-ColorDetector"
#define WIFI_PASSWORD       "colors1234"

// LED pins (per-board defaults, override here)
// ESP32:      R=25, G=26, B=27
// ESP32-C3:   R=3,  G=4,  B=5
// ESP8266:    R=5(D1), G=4(D2), B=14(D5)

// LED polarity
#define LED_COMMON_ANODE    0   // Set 1 for common-anode LED

// PWM & color
#define LED_GAMMA_CORRECTION  1 // Perceptual brightness
#define LED_SMOOTHING_FACTOR  0.08f // ~120ms ease
#define LED_IDLE_TIMEOUT_S    15  // Fade to off after no updates
```

### Customizing Pins / SSID at Build Time

```bash
# PlatformIO
pio run -e esp32dev -D LED_PIN_R=32 -D LED_PIN_G=33 -D LED_PIN_B=25 \
                    -D WIFI_SSID=\"MyColor\" -D WIFI_PASSWORD=\"secret123\"

# Arduino IDE: Add to "Build Options" → "Compiler Flags"
```

## API Reference

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Serves gzipped web app |
| `/api/status` | GET | Board info, client count, uptime, pins, security |
| `/api/color` | POST | Set LED color: body `r,g,b` (0–255) |
| `/api/color` | GET | Set LED color: `?r=255&g=128&b=0` |
| `/api/led?hex=RRGGBB` | GET | Quick test via curl |
| `/ws` | WS/WSS | Real-time color: send `c:RRGGBB`, `b:0-100` |

### Example: curl

```bash
# Set LED to cyan
curl -X POST http://192.168.4.1/api/color -d "0,255,255"

# Or via hex
curl "http://192.168.4.1/api/led?hex=00FFFF"

# Query status
curl http://192.168.4.1/api/status
```

## Building the Web Assets

The front-end lives in [`web/`](web/). A Python script inlines, minifies, and gzips it into `src/web_assets.h`.

```bash
# Regenerate web_assets.h after editing web/
python3 tools/build_web.py
```

This runs automatically as a PlatformIO pre-build script.

## Regenerating TLS Certificate

```bash
bash tools/gen_cert.sh
```

Creates a new ECDSA P-256 certificate valid ~10 years with SAN for `192.168.4.1` and `colors.local`.

## Project Structure

```
esp-color-detector/
├── platformio.ini          # PlatformIO config (4 environments)
├── README.md               # This file
├── src/
│   ├── main.cpp            # Entry point
│   ├── config.h            # All tunable settings
│   ├── led_controller.h/.cpp  # PWM, gamma, smoothing, animation
│   ├── web_server.h/.cpp   # Wi-Fi AP, HTTP/HTTPS, WS, API
│   ├── certs.h             # Self-signed TLS cert + key (ESP32)
│   └── web_assets.h        # Gzipped web app (auto-generated)
├── web/
│   ├── index.html          # App shell
│   ├── style.css           # Obsidian theme, glassmorphism
│   └── app.js              # Camera, color detection, WS client
├── tools/
│   ├── build_web.py        # Minify + gzip → PROGMEM header
│   └── gen_cert.sh         # Generate TLS certificate
└── docs/
    └── wiring.md           # Per-board wiring diagrams
```

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Camera won't open | Insecure context (HTTP) | Use ESP32 (HTTPS) or enable Chrome flag |
| LED colors swapped | Wrong pin mapping | Check `config.h` pins for your board |
| LED dim / inverted | Common-anode vs cathode | Set `LED_COMMON_ANODE=1` in `config.h` |
| Certificate error | Self-signed cert | Tap "Advanced → Proceed" (see above) |
| "No camera found" | Device has no rear camera | Use photo capture or manual picker |
| Build fails (ESP32) | Partition too small | Uses `huge_app.csv` (1.2 MB app) |
| WebSocket disconnects | Phone sleep / tab hidden | App auto-reconnects with exponential backoff |

## Browser Compatibility

| Browser | Live Camera | Photo Capture | Manual Picker |
|---------|-------------|---------------|---------------|
| Chrome Android | ✅ (HTTPS) | ✅ | ✅ |
| Firefox Android | ✅ (HTTPS) | ✅ | ✅ |
| Safari iOS | ✅ (HTTPS) | ✅ | ✅ |
| Chrome Desktop | ✅ (HTTPS) | ✅ | ✅ |
| All (HTTP only) | ❌ | ✅ | ✅ |

## Performance

| Metric | ESP32 | ESP32-S3 | ESP32-C3 | ESP8266 |
|--------|-------|----------|----------|---------|
| Flash used | ~480 KB | ~480 KB | ~480 KB | ~380 KB |
| RAM used | ~45 KB | ~45 KB | ~38 KB | ~28 KB |
| Web asset size | 14 KB gz | 14 KB gz | 14 KB gz | 14 KB gz |
| Color latency | <50 ms | <50 ms | <60 ms | ~80 ms |
| Max WS clients | 4 | 4 | 4 | 4 |

## License

MIT License — see [LICENSE](LICENSE) for details.

## Acknowledgments

- Arduino-ESP32 / Arduino-ESP8266 core teams
- [links2004/WebSockets](https://github.com/links2004/arduinoWebSockets) for ESP8266 WS
- Inspired by colorimeter hardware and mobile camera APIs

---

**Made with ❤️ for makers who love light and color.**