# B.HIVE — Load & Weigh Subsystem

Part of the **B.HIVE Autonomous Luggage Concierge** system, a capstone project at the Singapore University of Technology and Design (SUTD).

This repository contains the embedded firmware for the load and weigh station — the subsystem responsible for accepting, weighing, and staging luggage for pickup by the AMR.



## Related

- [B.HIVE AMR](https://github.com/friyk/2026S23CapstoneBHIVEAMR) — ROS 2 software stack for the autonomous mobile robot


## Overview

The firmware runs on a microcontroller and is built using PlatformIO. It interfaces with load cell sensors to measure luggage weight, and communicates status within the B.HIVE distributed system.

## Repository Structure

| Directory | Description |
|---|---|
| `src/` | Main firmware source code |
| `include/` | Header files |
| `lib/` | External/vendor libraries |
| `test/` | Unit tests |

## Tech Stack

- **Language:** C / C++
- **Build System:** PlatformIO
- **Hardware:** Microcontroller (MCU) with load cell interface and linear actuators

---

# Load Cell Weighing — ESP32 + HX711

## Overview

This module reads luggage weight using four load cells wired in a parallel summing configuration, read by a single HX711 24-bit ADC. The calibration factor is stored in EEPROM so it survives power cycles.

The system uses the `bogde/HX711_ADC` library and is designed to run non-blocking alongside the stepper motor module in the same main loop.

---

## Hardware

### Components
- ESP32 WROOM-32 DevKit
- HX711 ADC module
- 4x load cells (wired in parallel summing bridge)
- Acrylic platform with rollers mounted on top of the load cells

### Wiring

#### HX711 to ESP32

| HX711 Pin | ESP32 Pin |
|-----------|-----------|
| DOUT (DT) | GPIO 5    |
| SCK (SCK) | GPIO 18   |
| VCC       | 3.3V      |
| GND       | GND       |

#### Load Cells to HX711

All four load cells are wired in parallel — all wires of the same colour are connected together to the corresponding HX711 input:

| Wire Colour | HX711 Terminal |
|-------------|----------------|
| Red         | Red            |
| Black       | Black          |
| White       | White          |
| Green       | Green          |

> **Why parallel wiring works:** The platform is a rigid body, so the four load cells always carry forces that sum to the total luggage weight regardless of where the bag sits (F1 + F2 + F3 + F4 = W, always). With four identical load cells in parallel, the HX711 reads a signal proportional to the average force across all cells — which is proportional to the total weight. Position of the luggage on the platform does not affect the reading.

---

## How It Works

### Calibration Factor
The HX711 returns a raw ADC count. The calibration factor converts this to grams:

```
weight_grams = raw_ADC / cal_factor
```

The cal factor is determined once during calibration mode by placing a known mass on the platform and recording the raw reading. It is saved to EEPROM address 0 and loaded automatically on every boot.

### Operating Mode
The mode is set at compile time in `loadcell.h`:

```cpp
#define MODE READ       // normal operation
#define MODE CALIBRATE  // first-time calibration
```

In `READ` mode, the firmware loads the saved cal factor from EEPROM and prints weight every 2 seconds.

In `CALIBRATE` mode, the cal factor is set to 1.0 (raw units) and the calibration routine is activated. Switch back to `READ` and re-flash after calibration is complete.

### EEPROM
| Data | EEPROM Address | Size |
|------|----------------|------|
| Calibration factor | 0 | 4 bytes |

---

## Calibration Procedure

Calibration only needs to be done once, or when the load cells or platform are changed.

1. Set `#define MODE CALIBRATE` in `loadcell.h`, then erase and re-flash
2. Open serial monitor at 115200 baud
3. Ensure platform is **empty**, send `t` to tare
4. Enter your known weight in grams and press Enter (e.g. `20000` for 20 kg)
5. Place the known weight anywhere on the platform (ideally in the center)
6. Send `r` — the firmware waits 8 seconds for load cell creep to settle, then collects 30 samples automatically
7. Review the printed cal factor
8. Send `y` to save to EEPROM, or any other key to discard and repeat from step 5
9. Set `#define MODE READ` in `loadcell.h` and re-flash (Do not erase)

> **Why the 8 second wait:** Load cell creep — the strain gauge material slowly deforms immediately after a weight is placed, causing readings to drift upward for several seconds. The delay allows the fast initial creep to pass before samples are taken, giving a stable average.

> **Known limitation:** With a single HX711, positional errors of up to ~15% can occur if luggage sits significantly off-centre, due to the acrylic platform flexing under load and partially unloading some cells. For best accuracy, ensure luggage is roughly centred on the platform. A stiffer platform material (e.g. aluminium) would reduce this significantly.

---

## Configuration

Tunable values at the top of `loadcell.cpp`:

```cpp
const unsigned long PRINT_INTERVAL  = 2000;  // ms — how often weight is printed in READ mode
static const unsigned long CAL_SETTLE_MS   = 8000; // ms — settle wait before sampling in calibration
static const int           CAL_NUM_SAMPLES = 30;   // number of samples averaged per calibration point
```

---

# Stepper Motor Control — ESP32 + DM542 + AccelStepper

## Overview

This module controls two stepper motors via DM542 stepper drivers connected to an ESP32 WROOM-32. The motors are used to push luggage off a set of rolling pins after a weighing operation, then automatically or manually retract back to their home position.

The system is built around the `waspinator/AccelStepper` library and is designed to run non-blocking alongside the load cell module in the same main loop.

The code is implemented through PlatformIO. All commands written in the Readme.md are written through PlatformIO CLI.
```
pio run --target erase && pio run -t upload
```


---

## Hardware

### Components
- ESP32 WROOM-32 DevKit
- 2x DM542 Stepper Motor Drivers
- 2x Stepper Motors (worm gear driven linear rail)
- 24V Power Supply

### Wiring

Both motors are wired to the **same** STEP and DIR pins on the ESP32, receiving identical signals simultaneously:

| Signal       | Both DM542 #1 and #2 |
|--------------|----------------------|
| PULSE (PUL+) | GPIO 32 (D32)        |
| DIR (DIR+)   | GPIO 33 (D33)        |
| PUL- / DIR-  | GND                  |

> **Note:** ESP32 GPIO outputs 3.3V logic. The DM542 optocoupler inputs accept 3.3V directly without resistors — no level shifting required.

### DM542 Dip Switch Settings

| Switch | Position | Function | Value |
|--------|----------|----------|-------|
| SW1    | ON       | Current  | —     |
| SW2    | ON       | Current  | —     |
| SW3    | OFF      | Current  | 2.84A peak output current |
| SW4    | ON       | Idle current | 90% — full current at standstill |
| SW5    | ON       | Microstep | — |
| SW6    | OFF      | Microstep | — |
| SW7    | ON       | Microstep | — |
| SW8    | ON       | Microstep | 1600 pulses/rev |

> **Note on SW4:** Setting SW4=ON keeps current at 90% when idle. If motor heat is a concern, set SW4=OFF to drop idle current to 50%, which significantly reduces motor temperature between pushes.

---

## How It Works

### Coordinate System
Each motor tracks its position in **steps** from an arbitrary zero point set at startup. The key positions are:

- **Home** — the starting position of the motor on the rail, saved to EEPROM
- **Home + TRAVEL_STEPS** — the fully extended (pushed) position

`push` and `retract` move between these two positions. `unload` does the full cycle automatically.

### State Machine
The motor control runs as a state machine inside `stepper_update()`, which is called every `loop()` iteration. States are:

```
IDLE → PUSHING → IDLE
IDLE → RETRACTING → IDLE
IDLE → UNLOADING → UNLOAD_PAUSE (2s) → UNLOAD_RETRACTING → IDLE
```

The non-blocking design means the load cell continues reading weight during all motor movements.

### EEPROM Persistence
Home position and last known motor position are saved to EEPROM so they survive power cycles:

| Data | EEPROM Address | Size |
|------|----------------|------|
| Load cell cal factor | 0 | 4 bytes |
| Motor home position | 10 | 4 bytes |
| Motor last position | 14 | 4 bytes |

On boot, the ESP32 loads the last known position of the motor (not the home position) so it knows where the motor physically stopped. If it is not at home, a warning is printed and `retract` can be used to recover.

---

## Configuration

All tunable values are at the top of `stepper.cpp`:

```cpp
const float MAX_SPEED    = 11200;  // steps/sec — calculated based on desired time
const float ACCELERATION = 2500;   // steps/sec^2 — fixed at 2500, can be lower but not recommended to go higher
const long  TRAVEL_STEPS = 112000; // steps — calculated from measured distance
```

### Calculating TRAVEL_STEPS and MAX_SPEED

At 1600 pulses/rev, every full revolution moves the actuator **10mm**. Use this to convert your desired travel distance directly to steps:

```
TRAVEL_STEPS = (distance_mm / 10) × 1600
```

**Example — 70cm (700mm) travel:**
```
TRAVEL_STEPS = (700 / 10) × 1600 = 112,000 steps
```

Once you have `TRAVEL_STEPS`, calculate `MAX_SPEED` based on how many seconds you want the move to take. Because of the acceleration ramp, the average speed is slightly lower than `MAX_SPEED` — for most cases the following is a good approximation:

```
MAX_SPEED = TRAVEL_STEPS / desired_time_seconds
```

**Example — 112,000 steps in 10 seconds:**
```
MAX_SPEED = 112,000 / 10 = 11,200 steps/sec
```

Increase `MAX_SPEED` slightly (10–15%) to account for the acceleration ramp if the move is taking longer than expected.

### Independent Motor Tuning

If a single motor needs to be jogged independently (e.g. to set home position), physically unplug the motor that does **not** require tuning from the breadboard, then use the `m1<steps>` command to move the connected motor only. Replug when done.

```
m11600   → moves connected motor forward 1 revolution (10mm)
m1-1600  → moves connected motor backward 1 revolution (10mm)
```

---

## Serial Commands

Open the serial monitor at **115200 baud**. Commands are sent by typing and pressing Enter.
```
pio device monitor --baud 115200
```

### Combined Commands (both motors)

| Command   | Description |
|-----------|-------------|
| `push`    | Motor extends forward by `TRAVEL_STEPS` from home |
| `retract` | Motor returns to home position |
| `unload`  | Full automatic cycle: push → 2 second pause → retract |
| `stop`    | Emergency stop immediately. Send `retract` after to return home |
| `sethome` | Lock current position as home. Saved to EEPROM — persists across power cycles |
| `p`       | Print current step position of motor |

### Independent Commands (one motor at a time)

> **Note:** `m2` is disabled in the current firmware as both motors share GPIO 32/33. To re-enable Motor 2 independently, assign separate GPIO pins (originally 22/23) and uncomment the `motor2` lines in `stepper.cpp`.

| Command      | Description |
|--------------|-------------|
| `m1<steps>`  | Move motor 1 by N steps relative to current position. Positive = forward, negative = backward. Example: `m11600`, `m1-800` |
| `m2<steps>`  | Disabled — prints a reminder to re-enable in `stepper.cpp` |

### Load Cell Commands

| Command | Description |
|---------|-------------|
| `t`     | Tare / zero the scale |
| `r`     | Flip weight sign (use if weight reads as negative) |

---

## Typical Startup Sequence

### First-time setup (motors not yet at home)
1. Power on — motor loads last saved position from EEPROM
2. Use `m1<steps>` to jog motor to its correct starting position on the rail
3. Send `sethome` to lock and save this position
4. Send `t` to tare the load cell
5. System is ready

### Normal operation (after home is set)
1. Power on — home and last position load automatically from EEPROM
2. If WARNING appears (motor not at home), send `retract` to recover
3. Place luggage on rollers and weigh
4. Send `unload` to push luggage off and auto-retract
5. Repeat from step 3

---

## Changelog

### v1.0 — Initial single-motor implementation
- Single `stepper.cpp` / `stepper.h` module added to project
- Used `AccelStepper` library (`waspinator/AccelStepper`)
- Basic back-and-forth motion with `go` / `stop` commands
- Travel distance controlled by `TRAVEL_STEPS` constant
- Pins: GPIO 32 (STEP), GPIO 33 (DIR)

### v1.1 — Second motor added
- Added Motor 2 on GPIO 21 (STEP), GPIO 19 (DIR)
- Both motors move simultaneously on `push` / `retract`
- Commands changed from `go`/`stop` to `push`/`retract` to reflect luggage pusher purpose
- Independent `m1<steps>` / `m2<steps>` commands added for manual positioning
- `sethome` command added to lock starting positions

### v1.2 — Switched to MultiStepper for synchronisation
- Replaced dual `AccelStepper.run()` calls with `MultiStepper` to synchronise both motors
- Motors start and finish at the same time regardless of speed differences
- **Known issue:** MultiStepper has no acceleration support — motors slam to full speed instantly which can cause massive stalling
- Confirmed that ESP32 3.3V GPIO logic is sufficient to trigger DM542 optocoupler inputs directly — no resistors or level shifting required

### v1.3 — Reverted to AccelStepper, dropped MultiStepper
- MultiStepper's lack of acceleration caused motors to stall/jam on `push` at higher speeds
- Reverted to individual `AccelStepper` instances for both motors
- Both motors now wired to the same STEP (GPIO 32) and DIR (GPIO 33) pins — guaranteed identical signals and perfect synchronisation
- For independent motor tuning, unplug the motor not being tuned from the breadboard and use `m1<steps>`
- Both motors given identical `moveTo()` targets so they stay synchronised naturally
- Acceleration now works correctly — motors ramp up smoothly
- `ACCELERATION` fixed at 2500 steps/sec² — no longer user-tunable

### v1.4 — `unload` command added
- `TRAVEL_STEPS` and `MAX_SPEED` are now calculated from measured distance and desired time rather than trial and error
- New `unload` command: push → 2 second pause → auto retract
- Implemented as additional states in the state machine: `UNLOADING` → `UNLOAD_PAUSE` → `UNLOAD_RETRACTING`
- Pause is non-blocking using `millis()` — load cell continues reading during pause

### v1.6 — EEPROM persistence for home positions
- `sethome` now saves home positions to EEPROM (addresses 10 and 14)
- On boot, home positions are loaded automatically — no need to rejog after power cycle
- On boot, `currentPosition` set to home so `retract` works immediately

### v1.7 — Last known position tracking
- Added separate EEPROM storage for last known motor positions (addresses 18 and 22)
- On boot, `currentPosition` now set to last known position (not home) — accurately reflects where motors physically stopped
- If motors are not at home on boot, WARNING message printed and user prompted to send `retract`
- Position saved to EEPROM only when retract completes — avoids flash corruption from writes during motion

### v1.8 — EEPROM initialisation fix
- `EEPROM.begin(512)` was being called in both `loadcell.cpp` and `stepper.cpp` — potential conflict on ESP32
- Moved single `EEPROM.begin(512)` call to `main.cpp` before all module inits
- Removed `EEPROM.begin()` and `#include <EEPROM.h>` from `loadcell.cpp`
- Eliminated periodic EEPROM writes inside `stepper_update()` which were causing watchdog crashes on reboot

### v2.0 — Calibration Overhaul
- Replaced 5-point multi-position calibration (centre + 4 corners) with a single-point calibration
- Added 8-second settling delay after weight placement to allow load cell creep to stabilise before sampling
- Replaced single snapshot read (`refreshDataSet` / `getNewCalibration`) with a 30-sample average for a more accurate cal factor
- Increased HX711 startup stabilisation time from 2000ms to 3000ms
- Calibration weight can be placed anywhere on the platform — position does not affect the result due to summing bridge wiring

### v2.1 — Stepper Motor Double-Step Fix
- Identified root cause of push overshoot: both `AccelStepper` instances (`motor1` and `motor2`) shared the same GPIO pins (32/33), causing both to generate step pulses on the same pin simultaneously during `push`, effectively doubling the step count (~224,000 instead of 112,000)
- Disabled `motor2` (commented out throughout `stepper.cpp`) to eliminate double-stepping
- `motor2` code is fully preserved — re-enable by assigning separate GPIO pins (originally 22/23) and uncommenting the relevant lines in `stepper.cpp`
- Simplified EEPROM to a single home/position address pair (addresses 10 and 14)
