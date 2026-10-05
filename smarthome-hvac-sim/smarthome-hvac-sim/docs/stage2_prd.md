# Stage 2 - Project Requirements Document (PRD) & Development Plan

## 1. Functional requirements
| ID | Requirement |
|---|---|
| FR-1 | Driver exposes `/dev/smarthome`; `read()` returns one 16-byte `sh_event` (timestamp, temp, motion, hvac). |
| FR-2 | `read()` blocks until data, or returns `-EAGAIN` with `O_NONBLOCK`; `poll()` reports `POLLIN`. |
| FR-3 | ioctls: set HVAC, set target, set outdoor temp, force occupancy, get status. Invalid values return `-EINVAL`. |
| FR-4 | Driver simulates room thermal physics and random occupancy every `tick_ms`. |
| FR-5 | Daemon derives *occupied* from motion within `vacancy_timeout`. |
| FR-6 | Occupied: hold target +/- 0.5 C. Vacant: eco band target +/- 4 C. |
| FR-7 | Controller states Idle / Heating / Cooling / Fault with hysteresis (no chatter). |
| FR-8 | Fault if temp < 5 C or > 40 C: HVAC off, alert logged; recover with 1 C margin. |
| FR-9 | Async CSV logging of every sample plus state-change events. |
| FR-10 | CLI options (`--dev/--sim/--target/--vacancy-timeout/--duration/--log/--tick`). |
| FR-11 | On SIGINT/SIGTERM: stop threads, turn HVAC off, flush log. |
| FR-12 | `--sim` backend behaves like the driver, so the daemon runs without root. |

## 2. Non-functional requirements
| ID | Requirement |
|---|---|
| NFR-1 | No kernel warnings/oops on load, unload, or under concurrent readers. |
| NFR-2 | Reader slower than producer must not block the timer: ring overwrites oldest, counts `dropped`. |
| NFR-3 | Modules unload cleanly (timer stopped before device removal). |
| NFR-4 | User space builds with `-Wall -Wextra` with zero warnings; clean under TSan and ASan/UBSan. |
| NFR-5 | Control logic is pure and unit-testable without a device. |
| NFR-6 | Latency: command issued within one tick of the reading. |
| NFR-7 | Documented, Git-versioned, reproducible with `make`. |

## 3. Modules and deliverables
| Module | Deliverable |
|---|---|
| M1 Driver | `smarthome_drv.c`, `smarthome_ioctl.h`, Makefile |
| M2 Source abstraction | `ISource`, `DeviceSource`, `SimSource` |
| M3 Controller | `HvacController` + unit tests |
| M4 Concurrency & logging | `BlockingQueue`, `Logger`, `main.cpp` |
| M5 Docs | Stage docs, UML, test report, final report |

## 4. Timeline (mapped to stages)
| Stage | Work | Evidence |
|---|---|---|
| 1 | Idea, scope | `stage1` doc |
| 2 | PRD, plan | this document |
| 3 | Architecture, UML, env, Git | `stage3` doc, tag `stage-3` |
| 4 | Driver skeleton + daemon reading, prototype | tag `stage-4`, demo log |
| 5 | Remaining features, tests, sanitizers, fixes | tag `stage-5`, test report |
| 6 | Final demo, report, presentation | tag `v1.0` |

## 5. Risks
| Risk | Mitigation |
|---|---|
| Kernel API differs by version | `LINUX_VERSION_CODE` guards; test in a VM, not on the host |
| Driver crash freezes machine | Use a VM snapshot; small commits |
| Race conditions | Single spinlock, TSan on user space |
