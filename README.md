# B.HIVE Load & Weigh — Weighing and Unloading Station

B.HIVE Load & Weigh is a stationary model that simulates the weighing and luggage unloading subsystem of the B.HIVE autonomous mobile robot. An ESP32 reads four parallel-wired load cells through an HX711 ADC to measure luggage weight, then drives a stepper-powered linear pusher to offload bags — all running non-blocking in a single cooperative loop. In the final product design, this subsystem would be integrated directly into the AMR chassis.

---

## Related Repositories

| Repository | Description |
|---|---|
| **[B.HIVE AMR](https://github.com/friyk/2026S23CapstoneBHIVEAMR)** | ROS 2 software stack for the autonomous mobile robot — differential-drive control, AMCL localisation, Nav2 navigation, and BLE communication on a Jetson Orin Nano. This is the functional prototype. |
| **B.HIVE Load & Weigh** *(this repo)* | ESP32 firmware for a stationary model that simulates the robot's onboard weighing platform and stepper-driven luggage pusher. |

---

## What This Project Demonstrates

- **Sensor interfacing and calibration** — HX711 24-bit ADC driving four load cells in a parallel summing bridge, with a single-point calibration routine that accounts for load cell creep by settling and averaging 30 samples.
- **Non-blocking embedded control** — a cooperative main loop where the load cell and stepper state machine both run without blocking each other, using `millis()`-based timing throughout.
- **Stepper motion control** — AccelStepper-driven DM542 stepper drivers with trapezoidal acceleration profiles, a state machine handling push/pause/retract sequences, and EEPROM-persisted home and last-known positions for crash recovery.
- **EEPROM state persistence** — calibration factor, motor home position, and last-known motor position all survive power cycles, so the system can recover gracefully on reboot.
- **Iterative hardware-software co-design** — the [changelog](#changelog) documents the full evolution from single-motor prototype through MultiStepper synchronisation issues, double-step debugging, and calibration overhauls.

---

## System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 WROOM-32                            │
│                                                             │
│  loadcell module              stepper module                │
│  ┌──────────────┐            ┌──────────────────┐           │
│  │ HX711 ADC    │            │ AccelStepper     │           │
│  │ (GPIO 5, 18) │            │ (GPIO 32, 33)    │           │
│  └──────┬───────┘            └────────┬─────────┘           │
│         │                             │                     │
│    4× load cells              2× DM542 drivers              │
│    (parallel summing)         (shared STEP/DIR)             │
│                                       │                     │
│                               2× stepper motors             │
│                               (worm gear linear rail)       │
│                                                             │
│  ┌──────────────────────────────────────────────┐           │
│  │            EEPROM (512 bytes)                │           │
│  │  addr 0:  cal factor    (4 bytes)            │           │
│  │  addr 10: motor home    (4 bytes)            │           │
│  │  addr 14: motor lastpos (4 bytes)            │           │
│  └──────────────────────────────────────────────┘           │
└─────────────────────────────────────────────────────────────┘

       weight_display.py (Tkinter, runs on host PC)
       └── serial @ 115200 baud ── /dev/ttyUSB0
```

---

## Hardware

### Components

| Component | Model / Spec | Notes |
|---|---|---|
| MCU | ESP32 WROOM-32 DevKit | 3.3 V logic, PlatformIO / Arduino framework |
| ADC | HX711 (24-bit) | DOUT → GPIO 5, SCK → GPIO 18 |
| Load cells | 4× (parallel summing bridge) | Mounted under acrylic roller platform |
| Stepper drivers | 2× DM542 | 1600 pulses/rev, 2.84 A peak, 90 % idle current |
| Stepper motors | 2× (worm gear linear rail) | 10 mm travel per revolution |
| Power | 24 V supply | For stepper drivers |

### Wiring

**HX711 → ESP32:** DOUT to GPIO 5, SCK to GPIO 18, powered from 3.3 V.

**Load cells → HX711:** All four cells wired in parallel — same-colour wires connected together to the corresponding HX711 terminal (Red, Black, White, Green). This works because the rigid platform distributes force across all cells such that their sum always equals the total weight, regardless of load position.

**DM542 drivers → ESP32:** Both drivers share STEP (GPIO 32) and DIR (GPIO 33) so they receive identical signals. PUL- and DIR- tied to GND. The ESP32's 3.3 V output drives the DM542 optocoupler inputs directly — no level shifting needed.

### DM542 DIP Switch Settings

| SW1 | SW2 | SW3 | SW4 | SW5 | SW6 | SW7 | SW8 |
|---|---|---|---|---|---|---|---|
| ON | ON | OFF | ON | ON | OFF | ON | ON |

This gives 2.84 A peak current, 90 % idle current, and 1600 pulses/rev. Set SW4 to OFF to reduce idle current to 50 % if motor heat is a concern.

---

## Repository Structure

```
├── src/
│   ├── main.cpp          Entry point — init, serial command dispatcher
│   ├── loadcell.cpp      HX711 driver, calibration routine, weight output
│   ├── loadcell.h        Mode selection (CALIBRATE / READ), function decls
│   ├── stepper.cpp       AccelStepper control, state machine, EEPROM persistence
│   └── stepper.h         Stepper function declarations
├── weight_display.py     Tkinter GUI — live weight display + serial command entry
├── platformio.ini        Build config (ESP32, libraries)
├── include/              Project-level headers
├── lib/                  Vendored libraries
└── test/                 Unit tests
```

---

## Dependencies

Managed by PlatformIO (declared in `platformio.ini`):

| Library | Version | Purpose |
|---|---|---|
| `olkal/HX711_ADC` | ^1.0.5 | Non-blocking HX711 load cell driver |
| `waspinator/AccelStepper` | ^1.64.0 | Stepper motor control with acceleration |

The ESP32 Arduino framework provides `EEPROM.h` and `Serial`.

---

## Getting Started

### Build and Flash

```bash
# First-time flash (erases EEPROM — required before first calibration)
pio run --target erase && pio run -t upload

# Subsequent flashes (preserves EEPROM calibration and home data)
pio run -t upload
```

### Serial Monitor

```bash
pio device monitor --baud 115200
```

---

## Calibration

Calibration maps the raw HX711 ADC output to grams. It only needs to be done once, or when the load cells or platform are changed.

**1.** Set `#define MODE CALIBRATE` in `loadcell.h`, then erase and flash.

**2.** Send `t` to tare with the platform empty.

**3.** Enter the known reference weight in grams (e.g. `20000` for 20 kg).

**4.** Place the weight on the platform, then send `r`. The firmware waits 8 seconds for load cell creep to settle, then collects 30 samples and computes the calibration factor.

**5.** Send `y` to save to EEPROM.

**6.** Set `#define MODE READ` in `loadcell.h` and flash again (without erasing).

The 8-second settling delay exists because strain gauge material slowly deforms immediately after loading — the wait lets the fast initial creep pass so the sampled average is stable.

**Known limitation:** With a single HX711, off-centre loads can cause up to ~15 % positional error due to the acrylic platform flexing and partially unloading some cells. A stiffer platform (e.g. aluminium) would reduce this significantly.

---

## Serial Commands

### Normal Operation (MODE = READ)

| Command | Description |
|---|---|
| `t` | Tare / zero the scale |
| `r` | Flip weight sign (if reading negative) |
| `unload` | Full cycle: push → 2 s pause → auto retract |
| `push` | Extend pusher to full travel |
| `retract` | Return pusher to home position |
| `stop` | Emergency stop — halts motor immediately |
| `sethome` | Lock current motor position as home (saved to EEPROM) |
| `p` | Print current motor position in steps |
| `m1<steps>` | Move motor by N steps (e.g. `m11600`, `m1-800`) |
| `m2<steps>` | Disabled — requires separate GPIO pins (see stepper.cpp) |

### Calibration Mode (MODE = CALIBRATE)

| Command | Description |
|---|---|
| `t` | Tare |
| `<number>` | Set known mass in grams (e.g. `20000`) |
| `r` | Record calibration point (settles 8 s, averages 30 samples) |
| `y` | Save calibration factor to EEPROM |

---

## Stepper State Machine

The motor control runs as a non-blocking state machine inside `stepper_update()`:

```
IDLE ──→ PUSHING ──→ IDLE
IDLE ──→ RETRACTING ──→ IDLE
IDLE ──→ UNLOADING ──→ UNLOAD_PAUSE (2 s) ──→ UNLOAD_RETRACTING ──→ IDLE
```

Motor position is persisted to EEPROM only when a retract completes (not during motion), avoiding flash wear from continuous writes.

### Motion Parameters

| Parameter | Value | Derivation |
|---|---|---|
| `TRAVEL_STEPS` | 112,000 | 700 mm ÷ 10 mm/rev × 1600 steps/rev |
| `MAX_SPEED` | 11,200 steps/s | 112,000 steps ÷ 10 s |
| `ACCELERATION` | 2,500 steps/s² | Fixed — higher values risk stalling |

To recalculate for a different travel distance: `TRAVEL_STEPS = (distance_mm / 10) × 1600`, then `MAX_SPEED = TRAVEL_STEPS / desired_seconds`. Increase MAX_SPEED by 10–15 % to compensate for the acceleration ramp if the move takes longer than expected.

---

## Weight Display GUI

`weight_display.py` is a Tkinter application that connects to the ESP32 over serial and provides a large live weight readout, a scrollable log of all serial output, and a command input field.

```bash
python3 weight_display.py
```

Requires `pyserial` (`pip install pyserial`). Defaults to `/dev/ttyUSB0` at 115200 baud — edit `PORT` and `BAUDRATE` at the top of the file if needed.

---

## Typical Workflow

### First-Time Setup

1. Flash with `--target erase` to clear EEPROM
2. Calibrate the load cell (see [Calibration](#calibration))
3. Jog the motor to its home position with `m1<steps>`, then send `sethome`
4. System is ready

### Normal Operation

1. Power on — home position and calibration factor load from EEPROM automatically
2. If a warning appears (motor not at home), send `retract` to recover
3. Place luggage on the roller platform — weight prints every 2 seconds
4. Send `unload` — pusher extends, pauses 2 seconds, then retracts automatically
5. Repeat

---

## Changelog

### v1.0 — Initial single-motor implementation
Single `stepper.cpp` / `stepper.h` module with basic back-and-forth motion using AccelStepper. GPIO 32 (STEP), GPIO 33 (DIR).

### v1.1 — Second motor added
Motor 2 on GPIO 21 / 19. Both motors move simultaneously on `push` / `retract`. Independent `m1` / `m2` commands and `sethome` added.

### v1.2 — MultiStepper synchronisation
Replaced dual `AccelStepper.run()` with `MultiStepper` for synchronised motion. **Known issue:** MultiStepper has no acceleration support — motors slam to full speed causing stalling.

### v1.3 — Reverted to AccelStepper
MultiStepper dropped due to stalling. Both motors wired to same STEP/DIR pins (GPIO 32/33) for guaranteed identical signals. Acceleration works correctly again.

### v1.4 — Unload command
`unload` added: push → 2 s pause → auto retract. Non-blocking pause using `millis()`. `TRAVEL_STEPS` and `MAX_SPEED` now calculated from measured distance.

### v1.6 — EEPROM persistence for home positions
`sethome` saves to EEPROM. Home positions load automatically on boot.

### v1.7 — Last known position tracking
Separate EEPROM storage for last-known motor position. On boot, motor position reflects where it physically stopped. Warning printed if not at home.

### v1.8 — EEPROM initialisation fix
Consolidated `EEPROM.begin(512)` into `main.cpp`. Eliminated periodic writes during motion that caused watchdog crashes.

### v2.0 — Calibration overhaul
Replaced 5-point multi-position calibration with single-point. Added 8 s settling delay and 30-sample averaging. HX711 startup stabilisation increased to 3000 ms.

### v2.1 — Double-step fix
Both AccelStepper instances sharing the same GPIO pins caused double step counts. Disabled `motor2` to fix. Code preserved — re-enable by assigning separate GPIO pins and uncommenting in `stepper.cpp`.

---

## License

No license specified. The HX711_ADC library is by olkal, and AccelStepper is by Mike McCauley / waspinator.
