# B.HIVE — Load & Weigh Subsystem

Part of the **B.HIVE Autonomous Luggage Concierge** system, a capstone project at the Singapore University of Technology and Design (SUTD).

This repository contains the embedded firmware for the load and weigh station — the subsystem responsible for accepting, weighing, and staging luggage for pickup by the AMR.

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

## Related

- [B.HIVE AMR](https://github.com/friyk/2026S23CapstoneBHIVEAMR) — ROS 2 software stack for the autonomous mobile robot
