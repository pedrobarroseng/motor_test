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

> **Note:** The pin assignments used in this project (especially pin 22 and 23 for buttons) are specific to boards with extended digital I/O, such as the Arduino Mega. Adaptation is required for boards like the Arduino Uno.

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

At the end of each `loop()` iteration, the current ADC reading (`leituraAtual`) is stored as `leituraInicial`. On the next iteration, if the absolute difference between these two readings exceeds 512 (half the ADC range of 0–1023), the system enters a locked state (`sistemaTravado = true`), cutting the motor signal and displaying a `"connection error"` message. This detects sudden, physically implausible jumps in the potentiometer value — characteristic of a disconnected or broken analog input.

```
|leituraAtual - leituraInicial| ≥ 512  →  LOCK
```

**2. Motor Arming Requirement**

The motor will not respond to the potentiometer until `motorArmado` is set to `true`. This flag is only set when the potentiometer ADC reading falls below `50` (approximately 0% throttle), ensuring the operator physically zeros the throttle before gaining control. This prevents a motor from starting at an arbitrary throttle value upon boot.

**3. Bidirectional Neutral Lock**

For bidirectional ESCs, an additional flag (`sinalLiberado`) requires the operator to center the potentiometer near the neutral position (ADC reading within ±30 of 512) before free control is granted. Until this condition is met, the firmware forces a 1500 µs signal regardless of the actual potentiometer reading. This prevents abrupt direction reversals on ESCs that require a neutral-pass initialization.

---

### Menu and ESC Selection

The `setup()` function enters a blocking `while` loop that implements a two-action button interface using a single button (`buttonESC`), distinguished by press duration:

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
4. If not locked:
   a. Check arming condition (ADC < 50)
   b. If armed:
      - Map ADC to PWM microseconds
      - Map ADC to percentage
      - Apply neutral lock (bidirectional only)
      - Update display
      - Write PWM to ESC
      - Send serial telemetry
   c. If not armed:
      - Hold minimum/neutral signal
      - Display arming instruction
5. Store current ADC as reference for next iteration
6. Delay 10 ms
```

---

## Signal Mapping

The `map()` function performs a linear interpolation from the ADC domain to the PWM and percentage domains.

### Unidirectional ESC

| Variable | Input Range | Output Range |
|---|---|---|
| `sinalFinal` (PWM) | 0 – 1023 | 1000 – 1900 µs |
| `porcentagem` | 0 – 1023 | 0 – 100 % |

> The maximum PWM is intentionally capped at 1900 µs rather than 2000 µs to provide a safety headroom below the absolute ESC maximum.

### Bidirectional ESC

| Variable | Input Range | Output Range |
|---|---|---|
| `sinalFinal` (PWM) | 0 – 1023 | 1000 – 2000 µs |
| `porcentagem` | 0 – 1023 | -100 – 100 % |

The center of the potentiometer (ADC ≈ 512) corresponds to 1500 µs — the standard neutral point for bidirectional ESCs.

---

## Display States

The Nokia 5110 renders four distinct screens throughout the program lifecycle:

| State | Trigger | Content |
|---|---|---|
| **Splash Screen** | Boot | `"MOTOR TEST"` centered, shown for 2 seconds |
| **ESC Selection Menu** | `setup()` while-loop | Mode selector with progress bar |
| **Arming Prompt** | Motor not yet armed | Instructions to zero the potentiometer |
| **Neutral Lock Warning** | Bidirectional, center not reached | Current percentage and target PWM |
| **Telemetry Screen** | Normal operation | ESC type, throttle %, PWM value, raw ADC |

---

## Serial Output

During normal operation, the firmware transmits one line per loop iteration over UART at 9600 baud in the following format:

```
<sinalFinal>|<leitura_potenciometro>
```

**Example:**
```
1450|461
1523|535
1601|614
```

This output can be monitored via the Arduino IDE Serial Monitor or plotted with the Serial Plotter for real-time visualization of the PWM signal versus raw ADC value.

---

## Known Limitations

- **`setup()` called from `loop()`:** Invoking `setup()` manually from within `loop()` upon a reset button press is non-standard Arduino practice. While functional, it does not perform a true hardware reset and may leave certain hardware states (e.g., the `Servo` timer) in an inconsistent condition. A hardware reset via the `RESET` pin or a watchdog timer would be a more robust alternative.

- **`sistemaTravado` is irreversible:** Once the disconnection lock is triggered, there is no in-firmware path to clear it without a physical reset. This is intentional as a safety measure but should be documented for the operator.

- **No EEPROM persistence:** The ESC type selected in the menu is not stored in non-volatile memory. Every power cycle requires the operator to re-select the mode.
