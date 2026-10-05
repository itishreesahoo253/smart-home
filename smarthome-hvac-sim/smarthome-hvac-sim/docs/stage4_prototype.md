# Stage 4 - Initial Implementation & Prototype

## Prototype scope
- Driver: load/unload, timer, ring buffer, blocking `read`, `poll`, ioctls.
- Daemon: `DeviceSource`/`SimSource`, controller, queue, logger.

## How to demonstrate
```bash
cd daemon && make && ./smarthome_daemon --sim --tick 50 --duration 20      # no root
./scripts/run_demo.sh 30                                                     # with driver
cat /dev/smarthome | xxd | head                                              # raw 16-byte events (needs reader perms)
```

## Progress log
| Step | Result | Issue / solution |
|---|---|---|
| Controller + unit tests | 31 checks pass | - |
| Sim run, 8 s at 20 ms tick | 486 log lines on stdout and in the CSV file; HVAC cycles Idle <-> Cooling | Logger dropped one line per loop during shutdown: rewrote drain loop |
| ThreadSanitizer run | Reported a race: `SimSource::hvac_` written by control thread, read by producer | Made it `std::atomic<Hvac>`; TSan clean afterwards |
| ASan + UBSan run | No reports | - |
| Kernel module build/load | **TODO: run on your VM and paste `dmesg` output / screenshot here** | - |

## Evidence checklist (attach before presenting)
- [ ] `git log --oneline --graph` screenshot
- [ ] `dmesg` lines: `smarthome: loaded` / `unloaded`
- [ ] Sample `demo_log.csv` (first 30 lines)
- [ ] Screenshot of daemon output showing state changes

## Next stage
Edge cases (fault, slow reader), concurrency testing, ioctl error paths, performance, cleanup.
