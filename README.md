# Motor Test — ESC Controller with Nokia 5110 Display

An Arduino-based Electronic Speed Controller (ESC) testing platform designed to drive and monitor brushless motors through a potentiometer interface, with real-time telemetry displayed on a Nokia 5110 LCD. The system supports both unidirectional and bidirectional ESC configurations and implements a layered safety architecture to prevent motor damage and unintended behavior.

---

## Table of Contents

- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Pin Mapping](#pin-mapping)
- [Dependencies and Libraries](#dependencies-and-libraries)
- [System Architecture](#system-architecture)
  - [Safety Layer](#safety-layer)
  - [Menu and ESC Selection](#menu-and-esc-selection)
  - [Motor Arming Sequence](#motor-arming-sequence)
  - [Control Loop](#control-loop)
- [Signal Mapping](#signal-mapping)
- [Display States](#display-states)
- [Serial Output](#serial-output)
- [Data Acquisition](#data-acquisition)
- [Known Limitations](#known-limitations)

---

## Overview

This firmware implements a bench-test environment for brushless motor ESCs. Rather than relying on a standard RC transmitter, the system uses an analog potentiometer as a manual throttle source, translating its position into PWM microsecond signals recognized by standard ESC protocols.

The project is structured around two core operational modes:

| Mode | Description | PWM Range |
|---|---|---|
| **Unidirectional** | Full-range throttle from minimum to maximum | 1000 – 1900 µs |
| **Bidirectional** | Symmetric control around a neutral center point | 1000 – 2000 µs |

---

## Hardware Requirements

| Component | Description |
|---|---|
| Arduino Mega 2560 (or compatible) | Main microcontroller |
| Nokia 5110 LCD (PCD8544) | 84×48 pixel monochrome display |
| ESC (Electronic Speed Controller) | Unidirectional or bidirectional type |
| Brushless DC Motor | Compatible with the chosen ESC |
| 10 kΩ Potentiometer | Analog throttle control input |
| 2× Momentary Push Buttons | ESC type selection and system reset |

> **Note:** The pin assignments used in this project (especially pins 22 and 23 for buttons) are specific to boards with extended digital I/O, such as the Arduino Mega. Adaptation is required for boards like the Arduino Uno.

---

## Pin Mapping

| Pin | Role | Mode |
|---|---|---|
| `A0` | Potentiometer (throttle) | `INPUT` (analog) |
| `8` | ESC signal (PWM output) | `OUTPUT` via `Servo` library |
| `9` | Nokia 5110 — RST | `OUTPUT` |
| `10` | Nokia 5110 — CE (chip enable) | `OUTPUT` |
| `11` | Nokia 5110 — DC (data/command) | `OUTPUT` |
| `12` | Nokia 5110 — DIN (data in) | `OUTPUT` |
| `13` | Nokia 5110 — CLK (clock) | `OUTPUT` |
| `22` | ESC type selection button | `INPUT_PULLUP` |
| `23` | System reset button | `INPUT_PULLUP` |

---

## Dependencies and Libraries

### 1. `SPI.h` *(Arduino Built-in)*
The **Serial Peripheral Interface** library provides communication between the Arduino and SPI-compatible peripherals. In this project, it serves as the underlying transport layer for the Nokia 5110 display driver, handling the synchronous clock and data lines (CLK and DIN).

### 2. `Adafruit_GFX.h` *(Adafruit Industries)*
A hardware-agnostic **graphics abstraction library** that provides a common API for drawing primitives — text, rectangles, lines, and filled shapes — across a wide range of display hardware. It decouples the drawing logic from the specific display driver, enabling portability. This project uses it to render text, the progress bar, and layout separators on the Nokia 5110 screen.

> Install via Arduino Library Manager: `Adafruit GFX Library`

### 3. `Adafruit_PCD8544.h` *(Adafruit Industries)*
The hardware-specific driver for the **Philips PCD8544 display controller**, which powers the Nokia 5110 LCD module. It implements the low-level SPI command protocol for the PCD8544, including contrast control, display initialization, and memory-mapped pixel buffering. The `display.display()` call flushes the internal buffer to the physical screen.

> Install via Arduino Library Manager: `Adafruit PCD8544 Nokia 5110 LCD library`

### 4. `Servo.h` *(Arduino Built-in)*
The **Servo library** generates PWM signals in the range of 1000 to 2000 microseconds — the standard pulse-width protocol used by RC ESCs and servos. The `motor.writeMicroseconds()` method provides precise pulse-width control, which is more suitable for ESC calibration than the angular `motor.write()` method.

---

## System Architecture

### Safety Layer

The firmware implements three distinct safety mechanisms:

**1. Potentiometer Disconnection Detection**

At the end of each `loop()` iteration, the current ADC reading (`leituraAtual`) is stored as `leituraInicial`. On the next iteration, if the absolute difference between these two readings exceeds 512 (half the ADC range of 0–1023), the system enters a locked state (`sistemaTravado = true`), cutting the motor signal and displaying an error message. This detects sudden, physically implausible jumps in the potentiometer value — characteristic of a disconnected or broken analog input.

```
|leituraAtual - leituraInicial| ≥ 512  →  LOCK
```

**2. Motor Arming Requirement**

The motor will not respond to the potentiometer until `motorArmado` is set to `true`. This flag is only set when the potentiometer ADC reading falls below `50` (approximately 0% throttle), ensuring the operator physically zeros the throttle before gaining control. This prevents a motor from starting at an arbitrary throttle value upon boot.

**3. Bidirectional Neutral Lock**

For bidirectional ESCs, an additional flag (`sinalLiberado`) requires the operator to center the potentiometer near the neutral position (ADC reading within ±30 of 512) before free control is granted. Until this condition is met, the firmware forces a 1500 µs signal regardless of the actual potentiometer reading. This prevents abrupt direction reversals on ESCs that require a neutral-pass initialization.

---

### Menu and ESC Selection

The `setup()` function enters a blocking `while` loop that implements a two-action button interface using a single button (`PIN_BUTTON_ESC`), distinguished by press duration:

| Interaction | Duration | Action |
|---|---|---|
| Short press | < 800 ms | Toggle ESC type (uni/bi-directional) |
| Long press | ≥ 1000 ms | Confirm selection and exit menu |

The confirmation progress is visualized on the display as a fill-bar that grows from left to right over the 1000 ms hold window, computed using Arduino's `map()` and `constrain()` functions:

```cpp
int progresso = map(millis() - tempoSegurando, 0, 1000, 0, 84);
progresso = constrain(progresso, 0, 84);
```

The number `84` corresponds to the horizontal pixel width of the Nokia 5110 display. A debounce delay of 20 ms is applied at the end of each menu cycle.

---

### Motor Arming Sequence

After menu confirmation, the firmware performs the ESC initialization handshake:

- **Unidirectional:** sends a 1000 µs signal (minimum throttle).
- **Bidirectional:** sends a 1500 µs signal (neutral/center).

This step is required by most commercial ESCs, which expect to receive the minimum (or neutral) signal on startup before accepting throttle commands.

---

### Control Loop

The main `loop()` follows this execution order on each iteration:

```
1. Read potentiometer (ADC 0–1023)
2. Check reset button → call setup() if pressed
3. Check disconnection delta → lock if exceeded
4. Send CSV header once (first iteration only)
5. If locked:
   - Hold motor at minimum signal
   - Display error screen
   - Report LOCKED state via serial
6. If not locked:
   a. Check arming condition (ADC < 50)
   b. If armed:
      - Map ADC to PWM microseconds
      - Map ADC to percentage
      - Apply neutral lock (bidirectional only)
      - Update display
      - Write PWM to ESC
      - Send CSV telemetry line via serial
   c. If not armed:
      - Hold minimum/neutral signal
      - Display arming instruction
      - Report DISARMED state via serial
7. Store current ADC as reference for next iteration
8. Delay 10 ms
```

---

## Signal Mapping

The `map()` function performs a linear interpolation from the ADC domain to the PWM and percentage domains.

### The PWM RC Protocol

Both modes follow the **standard hobby PWM RC protocol**, which originated in the 1970s with radio-controlled systems and has since been widely adopted by the ESC industry for drones, aircraft, and robotics. In this protocol, the ESC does not interpret positive or negative voltages — it measures only the **duration of each pulse** in microseconds and maps that duration to a motor command according to its internal firmware:

```
1000 µs ←————————————— 1500 µs ————————————→ 2000 µs
   |                       |                       |
minimum                  center                 maximum
```

The physical meaning of each point depends entirely on the ESC type. The same electrical pulse of 1000 µs means "stopped" on a unidirectional ESC and "maximum reverse" on a bidirectional one — the signal is identical; only the ESC's internal firmware interpretation differs.

It is worth noting that more modern ESC protocols such as DSHOT, Oneshot, and Multishot use different signal formats and ranges. This bench targets specifically the standard PWM RC protocol, which remains the most widely supported across commercial ESC models.

---

### Unidirectional ESC

| Variable | Input Range | Output Range | Resolution |
|---|---|---|---|
| `sinalFinal` (PWM) | 0 – 1023 | 1000 – 1900 µs | ~0.88 µs per ADC step |
| `porcentagem` | 0 – 1023 | 0 – 100 % | ~0.098% per ADC step |

The usable PWM range is **900 µs** (1000 to 1900 µs). The maximum is intentionally capped at 1900 µs rather than 2000 µs as a safety margin for bench testing — this means the ESC never receives the absolute maximum pulse, operating at approximately 95% of its full rated range. The practical effect of this cap is greater control resolution: each 1% of potentiometer travel corresponds to ~9 µs of PWM variation, giving the operator finer granularity over the motor speed.

---

### Bidirectional ESC

| Variable | Input Range | Output Range | Resolution |
|---|---|---|---|
| `sinalFinal` (PWM) | 0 – 1023 | 1000 – 2000 µs | ~0.98 µs per ADC step |
| `porcentagem` | 0 – 1023 | -100 – 100 % | ~0.195% per ADC step |

The full 1000–2000 µs range is used to preserve the **symmetric range around the 1500 µs neutral point**:

```
1000 µs → -100% (maximum reverse)   distance from neutral: 500 µs
1500 µs →    0% (neutral / stopped) distance from neutral:   0 µs
2000 µs → +100% (maximum forward)   distance from neutral: 500 µs
```

The ESC internally computes the deviation from neutral (`pulse - 1500 µs`) to determine direction and magnitude. Both directions are therefore symmetric in power delivery.

A throttle reading of -100% producing a PWM of 1000 µs may appear counterintuitive — a lower number representing higher reverse power. This is not a design flaw but a direct consequence of the RC PWM protocol convention. This behavior is deliberately preserved in the telemetry display to maintain full traceability between raw PWM values and physical ESC behavior.

Compared to the unidirectional mode, each side of the bidirectional range spans only 500 µs, meaning each 1% of potentiometer travel corresponds to ~5 µs of PWM variation — making the control more sensitive per unit of potentiometer movement.

---

## Display States

The Nokia 5110 renders five distinct screens throughout the program lifecycle:

| State | Trigger | Content |
|---|---|---|
| **Splash Screen** | Boot | `"MOTOR TEST"` and `"ESC Bench"`, shown for 2 seconds |
| **ESC Selection Menu** | `setup()` while-loop | Mode selector with progress bar |
| **Arming Prompt** | Motor not yet armed | Instructions to zero the potentiometer |
| **Neutral Lock Warning** | Bidirectional, center not reached | Current percentage, separator line, and target PWM |
| **Telemetry Screen** | Normal operation | ESC type, throttle %, separator, PWM value, separator, raw ADC |

---

## Serial Output

During normal operation, the firmware transmits data at **115200 baud** in CSV format. A header line is sent once per session immediately after the menu selection, followed by one data line per loop iteration (~10 ms interval).

### CSV Format

```
timestamp_ms,pwm_us,adc_raw,throttle_pct,esc_type,system_state
```

| Field | Description | Example |
|---|---|---|
| `timestamp_ms` | Time since boot in milliseconds (`millis()`) | `8040` |
| `pwm_us` | PWM signal sent to the ESC in microseconds | `1450` |
| `adc_raw` | Raw ADC reading from the potentiometer (0–1023) | `461` |
| `throttle_pct` | Mapped throttle percentage (0–100 or -100–100) | `45` |
| `esc_type` | Selected ESC mode | `UNI` or `BI` |
| `system_state` | Current system state | `ARMED`, `DISARMED`, `WAITING_NEUTRAL`, `LOCKED` |

### Example Output

```
timestamp_ms,pwm_us,adc_raw,throttle_pct,esc_type,system_state
8040,1000,0,0,UNI,ARMED
8109,1450,461,45,UNI,ARMED
8178,1523,535,52,UNI,ARMED
```

---

## Data Acquisition

This project includes a Python-based data acquisition and analysis pipeline for collecting and visualizing ESC test data.

### Requirements

```
pyserial
matplotlib
pandas
```

Install with:

```bash
pip install pyserial matplotlib pandas --break-system-packages
```

### Project Structure

```
motor_test/
├── Motor_test/
│   ├── Motor_test.ino       # Arduino firmware
│   ├── aquisicao.py         # Serial data acquisition script
│   └── analise.py           # Data analysis and plot generation
├── data/
│   ├── raw/                 # Auto-generated CSV files (gitignored)
│   └── samples/             # Curated representative CSVs and PNGs for publication
└── .gitignore
```

### Standardized Test Protocols

To ensure reproducible and comparable results across different ESC models, the following test protocols are recommended:

| Test | Description | Applicable Modes |
|---|---|---|
| **Manual Ramp** | Gradually rotate the potentiometer from 0% to 100% (or -100% to +100%) over ~5–10 seconds | UNI, BI |
| **Step (Positive)** | Hold at neutral for ~3 seconds, then abruptly rotate to maximum forward throttle | UNI, BI |
| **Step (Negative)** | Hold at neutral for ~3 seconds, then abruptly rotate to maximum reverse throttle | BI only |

### Usage

**Step 1 — Flash the firmware** using the Arduino IDE, then close the Serial Monitor.

**Step 2 — Run acquisition:**
```bash
python3 Motor_test/aquisicao.py
```
The script connects to `/dev/ttyUSB0` at 115200 baud, saves data to `data/raw/teste_YYYY-MM-DD_HH-MM-SS.csv`, and prints each line to the terminal. Press `Ctrl+C` to stop and close the serial port cleanly.

**Step 3 — Run analysis:**
```bash
python3 Motor_test/analise.py
```

The analysis script supports two modes:

**Mode 1 — Individual plot:** Enter the CSV filename and a label (e.g. `UNI` or `BI`). The script generates a three-panel plot (PWM, throttle %, and ADC raw vs. time) with the label in each subplot title, and saves it as a PNG alongside the CSV.

**Mode 2 — Comparison plot:** Enter two CSV filenames and their respective labels. The script aligns both datasets to start at t=0, clips them to the same duration, and overlays them in a three-panel comparative plot. The output PNG is saved as `comparacao_<file1>_vs_<file2>.png`.

> **Note:** Only `ARMED` state data is plotted in both modes. `DISARMED`, `WAITING_NEUTRAL`, and `LOCKED` rows are automatically filtered out before plotting.

**Step 4 — Curate results:**
Copy the most representative CSVs and PNGs from `data/raw/` to `data/samples/` for use in publications and repository sharing.

---

## Known Limitations

- **`setup()` called from `loop()`:** Invoking `setup()` manually from within `loop()` upon a reset button press is non-standard Arduino practice. While functional, it does not perform a true hardware reset and may leave certain hardware states (e.g., the `Servo` timer) in an inconsistent condition. A hardware reset via the `RESET` pin or a watchdog timer would be a more robust alternative.

- **`sistemaTravado` is irreversible:** Once the disconnection lock is triggered, there is no in-firmware path to clear it without a physical reset. This is intentional as a safety measure but should be documented for the operator.

- **No EEPROM persistence:** The ESC type selected in the menu is not stored in non-volatile memory. Every power cycle requires the operator to re-select the mode.

- **Single serial port dependency:** The Python acquisition script and the Arduino IDE Serial Monitor cannot access the port simultaneously. The Serial Monitor must be closed before running `aquisicao.py`.

- **Open-loop architecture:** The bench operates in open loop — it generates and records PWM command signals but has no feedback from the motor or ESC. It characterizes the command behavior of the system, not the mechanical response. Sensor integration (e.g. current sensor, RPM sensor) would be required for closed-loop characterization.

- **Bidirectional neutral dead zone:** The neutral lock mechanism requires the potentiometer ADC reading to be within ±30 of 512 before control is released. This creates a small dead zone around the center position in bidirectional mode, which may cause minor oscillations at the start of tests that begin near neutral. This behavior is visible in the telemetry data and should be accounted for when interpreting bidirectional test results.