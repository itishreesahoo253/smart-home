# Stage 1 - Project Introduction

**Title:** Smart Home Occupancy & HVAC Control Simulator

## Objective
Build a small but realistic embedded-Linux stack: a kernel driver that exposes virtual smart-home
hardware, and a C++ user-space service that controls heating/cooling based on room occupancy.

## Problem
Heating and cooling an empty room wastes energy, but naive on/off thermostats also chatter
(rapid switching) and cannot react to presence. Real hardware (PIR sensors, relays) is not always
available for learning, so the whole loop is simulated in a way that behaves like a real device.

## Scope
**In scope:** one room, one PIR sensor, one temperature sensor, one HVAC actuator (off/heat/cool),
character driver with blocking read/poll/ioctl, C++ control daemon, logging, tests, documentation.
**Out of scope:** real GPIO/I2C hardware, networking/cloud, GUI, multi-room control.

## Expected outcome
- `smarthome_drv.ko` creating `/dev/smarthome`.
- `smarthome_daemon` keeping the room within a comfort band when occupied and a wider eco band when vacant.
- CSV log, unit tests, UML, Git history, final report.

## Applications
Smart thermostats, building energy management, embedded Linux driver teaching, hardware-in-the-loop
test benches where the driver stands in for a missing sensor board.

## Training topics demonstrated
| Topic | Where |
|---|---|
| Linux kernel modules, char devices | `driver/smarthome_drv.c` |
| Kernel timers, wait queues, spinlocks, poll | driver |
| ioctl ABI design, user/kernel copy | `smarthome_ioctl.h`, `copy_to_user`, `get_user` |
| System programming (open/poll/read/ioctl, signals) | `sources.cpp`, `main.cpp` |
| C++ (RAII, classes, interfaces, templates, threads) | `daemon/` |
| Computer architecture (struct layout/padding, memory ordering, ring buffer, IRQ/softirq context) | ABI structs, `READ_ONCE`, spinlock use |
| Hardware/software boundary | driver simulates sensor+actuator registers as ioctl/read |
| Git, build systems, testing, debugging | Makefiles, TSan/ASan, `dmesg`, `strace` |
