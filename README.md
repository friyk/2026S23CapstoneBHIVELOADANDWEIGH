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
- **Hardware:** Microcontroller (MCU) with load cell interface

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

- **Home** — the starting position of each motor on the rail, saved to EEPROM
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
Home positions and last known motor positions are saved to EEPROM so they survive power cycles:

| Data         | EEPROM Address | Size   |
|--------------|----------------|--------|
| Load cell cal factor | 0      | 4 bytes |
| Motor 1 home | 10             | 4 bytes |
| Motor 2 home | 14             | 4 bytes |
| Motor 1 last position | 18    | 4 bytes |
| Motor 2 last position | 22    | 4 bytes |

On boot, the ESP32 loads the last known position of each motor (not the home position) so it knows where the motors physically stopped. If they are not at home, a warning is printed and `retract` can be used to recover.

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
| `push`    | Both motors extend forward by `TRAVEL_STEPS` from home |
| `retract` | Both motors return to home position |
| `unload`  | Full automatic cycle: push → 2 second pause → retract |
| `stop`    | Emergency stop both motors immediately. Send `retract` after to return home |
| `sethome` | Lock current positions as home. Saved to EEPROM — persists across power cycles |
| `p`       | Print current step position of both motors |

### Independent Commands (one motor at a time)

> **Note:** `m2` is only relevant if Motor 1 and Motor 2 are on **different** STEP/DIR pins (e.g. Motor 1 on GPIO 32/33 and Motor 2 on GPIO 22/23). In the current shared-pin setup where both motors use GPIO 32/33, `m2` and `m1` produce identical behaviour. To move a single motor independently, physically unplug the other motor from the breadboard and use `m1<steps>`.

| Command      | Description |
|--------------|-------------|
| `m1<steps>`  | Move motor 1 by N steps relative to current position. Positive = forward, negative = backward. Example: `m11600`, `m1-800` |
| `m2<steps>`  | Move motor 2 by N steps relative to current position. Example: `m21600`, `m2-800` |

### Load Cell Commands

| Command | Description |
|---------|-------------|
| `t`     | Tare / zero the scale |
| `r`     | Flip weight sign (use if weight reads as negative) |

---

## Typical Startup Sequence

### First-time setup (motors not yet at home)
1. Power on — motors load last saved positions from EEPROM
2. Use `m1<steps>` and `m2<steps>` to jog each motor independently to its correct starting position on the rail
3. Send `sethome` to lock and save these positions
4. Send `t` to tare the load cell
5. System is ready

### Normal operation (after home is set)
1. Power on — home and last position load automatically from EEPROM
2. If WARNING appears (motors not at home), send `retract` to recover
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





