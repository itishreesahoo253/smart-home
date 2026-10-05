# Stage 5 - Testing, Integration & Improvement

## Unit tests (`daemon/tests/test_controller.cpp`, `make test`)
| ID | Case | Expected | Result |
|---|---|---|---|
| TC-01 | 22 C occupied -> heat until 24 C | Heat, then Off/Idle | Pass |
| TC-02 | 26 C occupied -> cool until 24 C | Cool, then Off/Idle | Pass |
| TC-03 | Temperatures inside deadband | No switching (no chatter) | Pass |
| TC-04 | 26 C vacant vs. occupied | Off when vacant, Cool after motion | Pass |
| TC-05 | Vacancy timeout at 29 s / 31 s | Occupied, then vacant | Pass |
| TC-06 | 45 C, then 39.5 C, then 30 C | Fault, stays Fault, recovers | Pass |
| TC-07 | Custom target 20 C at 22 C | Cool | Pass |

Total: 31 assertions, 0 failures.

## Sanitizer / robustness runs (simulator backend)
| Tool | Finding | Fix |
|---|---|---|
| TSan | Data race on `SimSource::hvac_` | `std::atomic` |
| ASan + UBSan | Clean | - |
| Warnings (`-Wall -Wextra`) | None | - |

## Integration & system tests (run on Linux VM with the driver)
| ID | Steps | Expected | Result |
|---|---|---|---|
| IT-01 | `insmod`, check `/dev/smarthome`, `dmesg` | Node exists, "loaded" | TODO |
| IT-02 | `python3 -c "open('/dev/smarthome','rb').read(16)"` | 16 bytes after <= 1 tick | TODO |
| IT-03 | Open with `O_NONBLOCK` before first tick | `EAGAIN` | TODO |
| IT-04 | ioctl `SET_TARGET` with 5000 mC | `EINVAL` | TODO |
| IT-05 | Unknown ioctl number | `ENOTTY` | TODO |
| IT-06 | Do not read for 40 s | `dropped > 0` via GET_STATUS, timer keeps running | TODO |
| IT-07 | Run daemon 60 s | Temperature stays in band; log has state events | TODO |
| IT-08 | Ctrl-C daemon | HVAC set to OFF, log flushed | TODO |
| IT-09 | `rmmod` while daemon stopped, repeat 20x | No oops, no leak | TODO |
| IT-10 | Two readers concurrently | No crash, each event delivered once | TODO |

Fill the Result column with Pass/Fail after running on your machine.

## Improvements made
- Race removed (atomic), logger shutdown drain fixed, ring buffer overwrite policy protects the timer,
  input validation on every ioctl, RAII for fd.

## Known issues / ideas
- Single device instance (global state). - No `mmap`/sysfs interface. - Occupancy model is random, not scripted.
