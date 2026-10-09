# Wiring Guide

Per-board pinout tables and ASCII wiring diagrams for the ESP Color Detector.

---

## ESP32 DevKit V1 / ESP32-WROOM-32

### Pin Mapping

| Function | GPIO | Board Pin | Notes |
|----------|------|-----------|-------|
| Red LED  | 25   | D25       | PWM (LEDC channel 0) |
| Green LED| 26   | D26       | PWM (LEDC channel 1) |
| Blue LED | 27   | D27       | PWM (LEDC channel 2) |
| GND      | —    | GND       | Common cathode |

### ASCII Wiring Diagram

```
                    ESP32 DevKit V1
                   ┌─────────────────┐
                   │                 │
            3V3 ───┤                 │
                   │                 │
            GND ───┤                 ├── GND ──────────────────┐
                   │                 │                         │
            D25 ──┤            D26──┤                         │
                   │                 │                         │
            D27 ──┤                 │                         │
                   │                 │                         │
                   └─────────────────┘                         │
                                                               │
        ┌──────────────────────────────────────────────────────┘
        │
        ▼
   ┌─────────┐
   │  RGB    │
   │  LED    │
   │ (Common │
   │ Cathode)│
   └────┬────┘
        │
   ┌────┴────┐
   │         │
   R         G         B
   │         │         │
  [220Ω]   [220Ω]    [220Ω]
   │         │         │
   └────┬────┴────┬────┘
        │         │
       GND       GND
        │         │
        └────┬────┘
             │
            GND (shared)

Pinout (5mm common-cathode RGB LED, longest leg = cathode):
     ┌───┐
     │ █ │  ◄── Longest leg = Common Cathode (GND)
     ├───┤
     │ █ │  ◄── Red anode
     ├───┤
     │ █ │  ◄── Green anode
     ├───┤
     │ █ │  ◄── Blue anode
     └───┘

Resistors: 220 Ω (or 330 Ω) in series with EACH anode
```

### Notes

- **Avoid strapping pins:** GPIO 0, 2, 12, 15 (boot config) — not used here
- **Avoid input-only:** GPIO 34, 35, 36, 39 — not used here
- GPIO 25/26/27 are safe PWM outputs with LEDC
- If using common-anode LED: set `LED_COMMON_ANODE=1` in `config.h` and connect common pin to **3V3** instead of GND

---

## ESP32-S3 DevKitC-1

### Pin Mapping

| Function | GPIO | Board Pin | Notes |
|----------|------|-----------|-------|
| Red LED  | 25   | D25       | PWM (LEDC channel 0) |
| Green LED| 26   | D26       | PWM (LEDC channel 1) |
| Blue LED | 27   | D27       | PWM (LEDC channel 2) |
| GND      | —    | GND       | Common cathode |

### Wiring

**Identical to ESP32 DevKit V1** — same GPIO numbers, same LEDC channels.

```
ESP32-S3 DevKitC-1          RGB LED (Common Cathode)
─────────────────────────────────────────────────────
GPIO 25  ──[220Ω]──►  R (Red anode)
GPIO 26  ──[220Ω]──►  G (Green anode)
GPIO 27  ──[220Ω]──►  B (Blue anode)
GND      ──────────►  Common Cathode
```

---

## ESP32-C3 DevKitM-1

### Pin Mapping

| Function | GPIO | Board Pin | Notes |
|----------|------|-----------|-------|
| Red LED  | 3    | D3        | PWM (LEDC channel 0) |
| Green LED| 4    | D4        | PWM (LEDC channel 1) |
| Blue LED | 5    | D5        | PWM (LEDC channel 2) |
| GND      | —    | GND       | Common cathode |

### ASCII Wiring Diagram

```
                    ESP32-C3 DevKitM-1
                   ┌─────────────────┐
                   │                 │
            3V3 ───┤                 │
                   │                 │
            GND ───┤                 ├── GND ──────────────────┐
                   │                 │                         │
            D3  ──┤            D4 ──┤                         │
                   │                 │                         │
            D5  ──┤                 │                         │
                   │                 │                         │
                   └─────────────────┘                         │
                                                               │
        ┌──────────────────────────────────────────────────────┘
        │
        ▼
   ┌─────────┐
   │  RGB    │
   │  LED    │
   │ (Common │
   │ Cathode)│
   └────┬────┘
        │
   ┌────┴────┐
   │         │
   R         G         B
   │         │         │
  [220Ω]   [220Ω]    [220Ω]
   │         │         │
   └────┬────┴────┬────┘
        │         │
       GND       GND
        │         │
        └────┬────┘
             │
            GND
```

### Notes

- ESP32-C3 has fewer GPIOs; 3/4/5 are safe PWM outputs
- GPIO 2/3 are strapping pins but **GPIO 3 is safe** when not held low at boot (our LED pulls high via resistor)
- If using common-anode: connect common to **3V3**, set `LED_COMMON_ANODE=1`

---

## ESP8266 / ESP-12E (NodeMCU / Wemos D1 Mini)

### Pin Mapping (NodeMCU / Arduino Pin Names)

| Function | GPIO | NodeMCU Pin | Arduino Pin | Notes |
|----------|------|-------------|-------------|-------|
| Red LED  | 5    | D1          | 5           | PWM (analogWrite) |
| Green LED| 4    | D2          | 4           | PWM (analogWrite) |
| Blue LED | 14   | D5          | 14          | PWM (analogWrite) |
| GND      | —    | GND         | GND         | Common cathode |

### ASCII Wiring Diagram

```
                    NodeMCU / ESP-12E
                   ┌─────────────────┐
                   │                 │
            3V3 ───┤                 │
                   │                 │
            GND ───┤                 ├── GND ──────────────────┐
                   │                 │                         │
            D1 ───┤            D2 ──┤                         │
                   │                 │                         │
            D5 ───┤                 │                         │
                   │                 │                         │
                   └─────────────────┘                         │
                                                               │
        ┌──────────────────────────────────────────────────────┘
        │
        ▼
   ┌─────────┐
   │  RGB    │
   │  LED    │
   │ (Common │
   │ Cathode)│
   └────┬────┘
        │
   ┌────┴────┐
   │         │
   R         G         B
   │         │         │
  [220Ω]   [220Ω]    [220Ω]
   │         │         │
   └────┬────┴────┬────┘
        │         │
       GND       GND
        │         │
        └────┬────┘
             │
            GND
```

### ESP8266 PWM Notes

- Uses `analogWrite()` (software PWM via timers)
- Frequency: 1 kHz (set via `analogWriteFreq(1000)`)
- Range: 0–255 (set via `analogWriteRange(255)`)
- All 3 pins (5, 4, 14) support PWM simultaneously
- **No hardware LEDC** — smoother on ESP32 but perfectly usable

### ESP8266 Pin Constraints

| Pin | GPIO | Safe for LED? | Notes |
|-----|------|---------------|-------|
| D0  | 16   | ❌            | No PWM, connected to RST (deep sleep) |
| D1  | 5    | ✅            | **Red** — PWM OK |
| D2  | 4    | ✅            | **Green** — PWM OK |
| D3  | 0    | ❌            | Boot strapping (must be HIGH at boot) |
| D4  | 2    | ⚠️            | Boot strapping, built-in LED (inverted) |
| D5  | 14   | ✅            | **Blue** — PWM OK |
| D6  | 12   | ⚠️            | Boot strapping (must be LOW at boot) |
| D7  | 13   | ⚠️            | Boot strapping |
| D8  | 15   | ❌            | Boot strapping (must be LOW at boot) |

> **D1/D2/D5 (GPIO 5/4/14) are the only safe PWM pins** that don't interfere with boot.

---

## Common-Anode LED Wiring

If your RGB LED is **common-anode** (longest leg = anode, connects to VCC):

### Changes Required

1. **Set in `config.h`:**
   ```cpp
   #define LED_COMMON_ANODE  1
   ```

2. **Wiring:** Connect common pin to **3V3** (not GND)

```
                    ESP32 (any variant)
                   ┌─────────────────┐
                   │                 │
            3V3 ───┤                 ├── 3V3 ──────────────────┐
                   │                 │                         │
            GND ───┤                 │                         │
                   │                 │                         │
            D25 ──┤            D26──┤                         │
                   │                 │                         │
            D27 ──┤                 │                         │
                   └─────────────────┘                         │
                                                               │
        ┌──────────────────────────────────────────────────────┘
        │
        ▼
   ┌─────────┐
   │  RGB    │
   │  LED    │
   │(Common  │
   │ Anode)  │
   └────┬────┘
        │
   ┌────┴────┐
   │         │
   R         G         B
   │         │         │
  [220Ω]   [220Ω]    [220Ω]
   │         │         │
   └────┬────┴────┬────┘
        │         │
       GPIO      GPIO
        │         │
       (sink)    (sink)
```

### How It Works

- **Common-cathode (default):** ESP32 **sources** current → LED anode. HIGH = ON.
- **Common-anode:** ESP32 **sinks** current → LED cathode. LOW = ON (inverted).
- The firmware handles inversion automatically when `LED_COMMON_ANODE=1`

---

## Resistor Selection

| LED Type | Forward Voltage (typ) | Resistor (3.3V) | Resistor (5V*) |
|----------|----------------------|-----------------|----------------|
| Red      | 1.8–2.2 V            | 220 Ω           | 330 Ω          |
| Green    | 2.8–3.2 V            | 100–150 Ω       | 220 Ω          |
| Blue     | 2.8–3.4 V            | 100–150 Ω       | 220 Ω          |

> *ESP32/ESP8266 GPIOs are 3.3V logic. 5V only if using external level shifter.

**Recommended:** **220 Ω for all three** — simple, safe, works well enough.  
For better color balance: 220 Ω (Red), 150 Ω (Green), 150 Ω (Blue).

### Power Calculation (220 Ω @ 3.3V)

| Color | Vf | Current | Power (per resistor) |
|-------|-----|---------|---------------------|
| Red   | 2.0V | ~6 mA  | ~8 mW |
| Green | 3.0V | ~1.4 mA | ~0.3 mW |
| Blue  | 3.2V | ~0.5 mA | ~0.05 mW |

Total LED current: **~8 mA** — well within GPIO limits (12–20 mA per pin, 40–80 mA total).

---

## Breadboard Layout (Fritzing-style)

### ESP32 DevKit + RGB LED

```
    ESP32 DevKit                    Breadboard
   ┌──────────────┐                 ┌─────────────────────────┐
   │              │                 │                         │
   │  3V3    3V3  │                 │  +  +  +  +  +  +  +  + │
   │  GND    GND  │                 │  -  -  -  -  -  -  -  - │
   │  D25   D26   │                 │                         │
   │  D27         │                 │  [R] [G] [B]            │
   │              │                 │   │   │   │             │
   └──────────────┘                 │  220 220 220            │
         │                          │   │   │   │             │
         │                          │   ▼   ▼   ▼             │
    Jumper wires                    │  ┌─────────────┐        │
         │                          │  │  RGB LED    │        │
         ▼                          │  │  (Common    │        │
   ┌─────────────────────────┐     │  │   Cathode)  │        │
   │                         │     │  │      │      │        │
   │  a  b  c  d  e  f  g  h │     │  └──────┬──────┘        │
   │  1  2  3  4  5  6  7  8 │     │         │               │
   │                         │     │        GND              │
   └─────────────────────────┘     └─────────────────────────┘

Connections:
  D25 ────► Row 1, Col a ────► 220Ω ────► RGB LED Red anode
  D26 ────► Row 1, Col c ────► 220Ω ────► RGB LED Green anode
  D27 ────► Row 1, Col e ────► 220Ω ────► RGB LED Blue anode
  GND ────► Row 1, Col g ──────────────► RGB LED Common Cathode (longest leg)
```

---

## Schematic (KiCad-style Netlist)

```
Net: VCC_3V3
  ESP32:3V3
  (Common-anode only: RGB_LED:COMMON)

Net: GND
  ESP32:GND
  RGB_LED:CATHODE (common-cathode)
  R1:2, R2:2, R3:2

Net: LED_R
  ESP32:GPIO25 (or 3/5 per board)
  R1:1

Net: LED_G
  ESP32:GPIO26 (or 4/4 per board)
  R2:1

Net: LED_B
  ESP32:GPIO27 (or 5/14 per board)
  R3:1

Components:
  R1: 220Ω 0603/0805/Through-hole
  R2: 220Ω
  R3: 220Ω
  RGB_LED: Common-cathode 5mm (or SMD PLCC-4/6)
```

---

## Troubleshooting Wiring

| Symptom | Check |
|---------|-------|
| LED doesn't light | 1. Correct GPIO in `config.h`<br>2. Resistor not open<br>3. LED orientation (cathode = longest leg)<br>4. GND connected |
| Wrong colors | 1. R/G/B wires swapped<br>2. `LED_COMMON_ANODE` matches hardware |
| LED very dim | 1. Resistor too large (>1kΩ)<br>2. Common-anode wired to GND (should be 3V3) |
| Flickering | 1. Loose breadboard contact<br>2. Missing `yield()` in loop (ESP8266) |
| Boot fails | 1. LED on strapping pin (GPIO 0, 2, 12, 15)<br>2. Move to safe pins |

---

## Quick Verify Script

Upload this minimal test to verify wiring before flashing full firmware:

```cpp
// test_led.ino — Run once to verify LED wiring
#define R_PIN 25  // Change per board
#define G_PIN 26
#define B_PIN 27

void setup() {
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);
  Serial.begin(115200);
}

void loop() {
  // Red
  analogWrite(R_PIN, 255); analogWrite(G_PIN, 0); analogWrite(B_PIN, 0);
  Serial.println("RED"); delay(1000);
  // Green
  analogWrite(R_PIN, 0); analogWrite(G_PIN, 255); analogWrite(B_PIN, 0);
  Serial.println("GREEN"); delay(1000);
  // Blue
  analogWrite(R_PIN, 0); analogWrite(G_PIN, 0); analogWrite(B_PIN, 255);
  Serial.println("BLUE"); delay(1000);
  // White
  analogWrite(R_PIN, 255); analogWrite(G_PIN, 255); analogWrite(B_PIN, 255);
  Serial.println("WHITE"); delay(1000);
  // Off
  analogWrite(R_PIN, 0); analogWrite(G_PIN, 0); analogWrite(B_PIN, 0);
  Serial.println("OFF"); delay(1000);
}
```

Expected: LED cycles Red → Green → Blue → White → Off. If colors appear in wrong order, fix pin assignments in `config.h`.

---

## Bill of Materials (BOM)

| Part | Qty | Example Part | Est. Cost |
|------|-----|--------------|-----------|
| ESP32 DevKit V1 | 1 | ESP32-WROOM-32 DevKit | $5–8 |
| 5mm Common-Cathode RGB LED | 1 | WP7113ZGD / generic | $0.20 |
| 220 Ω Resistor (1/4W) | 3 | Through-hole or 0805 SMD | $0.01 |
| Breadboard (400-pin) | 1 | Generic | $2–4 |
| Jumper Wires (M-M) | 10+ | 20cm | $2–5 |
| Micro-USB Cable | 1 | Data+Power | $1–3 |

**Total: ~$10–20** (one-time; breadboard/wires reusable)

---

## Safety Notes

- **Always use resistors** — direct GPIO to LED burns the pin
- **Max GPIO current:** 12 mA (ESP32) / 12 mA (ESP8266) per pin
- **Total GPIO current:** 40 mA (ESP32) / 80 mA (ESP8266) combined
- Our design: ~8 mA total — well within limits
- **Never connect LED anode to 5V** — ESP32 GPIOs are 3.3V only
- **Disconnect USB** before rewiring

---

*For questions or issues, see [README.md](../README.md#troubleshooting) or open a GitHub issue.*