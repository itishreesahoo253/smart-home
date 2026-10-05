# Stage 6 - Final Report & Presentation

## 1. Summary
A kernel char driver simulating a PIR sensor, room thermometer and HVAC actuator, plus a
multithreaded C++17 daemon that controls climate using occupancy-aware comfort/eco bands.

## 2. Final architecture
See `stage3_design.md` (architecture, class, sequence, state diagrams).

## 3. Implementation highlights
- **Driver:** `timer_setup` + `mod_timer` periodic tick; spinlock-protected ring buffer;
  `wait_event_interruptible` + `wake_up_interruptible`; `poll_wait`; validated `ioctl`;
  `copy_to_user` outside the lock; ordered error unwinding in `init`; `timer_delete_sync` in `exit`.
- **Daemon:** interface-based design (`ISource`), RAII, templates (`BlockingQueue<T>`), `std::thread`,
  `std::atomic`, `poll(2)`, signal handling, pure/testable controller.
- **Architecture concepts:** fixed 16-byte ABI without padding surprises, softirq context rules
  (no sleeping, IRQ-safe locks), producer/consumer ring buffer, memory ordering (`READ_ONCE`/`WRITE_ONCE`).

## 4. Testing results
See `stage5_testing.md`. Unit tests 31/31; TSan and ASan/UBSan clean; hardware-in-VM tests: fill in.

## 5. Results
Attach a plot of `demo_log.csv`: temperature vs. time with the band and occupancy shading. Expected
behaviour: temperature held in a 1 C band when occupied, 8 C band when vacant, fewer HVAC cycles in eco mode.
Quick metric: `grep -c "IDLE -> " demo_log.csv` per mode.

## 6. Achievements
Complete driver-to-application stack; testable design; documented process with tagged stages.

## 7. Limitations
Simulated physics; single room; no persistence of settings; kernel module tested on limited kernel versions.

## 8. Future improvements
sysfs/debugfs attributes, device-tree/platform-driver version, real GPIO PIR + DHT/BME280 via I2C,
MQTT/REST interface, PID control, multiple rooms (per-minor devices), `mmap`ed ring buffer,
KUnit tests for the driver.

## 9. Presentation outline (10 min)
1. Problem (1) 2. Architecture (2) 3. Driver walk-through (2) 4. Daemon & state machine (2)
5. Live demo (2) 6. Testing, limits, future (1).

## 10. Likely interview questions
- Why a spinlock and not a mutex in the timer path? (softirq cannot sleep)
- Why `copy_to_user` outside the lock? (may fault and sleep)
- What if the reader is slow? (overwrite oldest, `dropped` counter)
- Why hysteresis? (prevent chatter/wear)
- Why `del_timer_sync` before destroying the device? (callback must not run after unload)
- `poll` vs. blocking `read`? (multiplex with timeout, allows clean shutdown)
- How would you port this to real hardware? (platform driver, GPIO/I2C, device tree)
