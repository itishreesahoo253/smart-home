# Smart Home Occupancy & HVAC Control Simulator

Individual capstone: **Linux device driver + system programming + C++**.

A Linux kernel module simulates a PIR occupancy sensor, a room temperature sensor and an HVAC
actuator (`/dev/smarthome`). A multithreaded C++17 daemon reads the sensors, decides heating /
cooling with an occupancy-aware state machine (comfort mode vs. eco mode) and logs everything.

```
 kernel:  timer -> thermal model -> ring buffer -> /dev/smarthome (read, poll, ioctl)
 user  :  producer thread -> queue -> HvacController -> ioctl(SET_HVAC) ; logger thread -> CSV
```

## Quick start (no root, no driver)
```bash
cd daemon && make && make test
./smarthome_daemon --sim --tick 50 --duration 20 --log run.csv
```

## Full run with the kernel driver
```bash
sudo apt install build-essential linux-headers-$(uname -r)
./scripts/run_demo.sh 60          # builds, insmod, runs 60 s, rmmod
```
Debug: `dmesg | tail`, `lsmod | grep smarthome`, `ls -l /dev/smarthome`, `strace -e ioctl,read,poll ./daemon/smarthome_daemon`.

## Layout
| Path | Purpose |
|---|---|
| `driver/` | Kernel module + shared ioctl ABI header |
| `daemon/` | C++ daemon, unit tests, Makefile |
| `docs/` | Stage 1-6 documents, PRD, UML, test report |
| `scripts/` | Git setup, demo runner |

## Stage documents
1. [Introduction](docs/stage1_introduction.md)
2. [Requirements & plan (PRD)](docs/stage2_prd.md)
3. [Design & architecture, UML, Git](docs/stage3_design.md)
4. [Prototype & progress log](docs/stage4_prototype.md)
5. [Testing & improvement](docs/stage5_testing.md)
6. [Final report & presentation](docs/stage6_final_report.md)

License: GPL-2.0 (kernel module) / MIT (user space).
