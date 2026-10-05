# Stage 3 - System Design & Architecture

## 1. Architecture
```mermaid
flowchart LR
  subgraph KERNEL[Kernel space]
    T[Kernel timer<br/>tick_ms] --> M[Thermal + occupancy model]
    M --> R[(Ring buffer 64 x sh_event)]
    R --> F[file_operations<br/>read / poll / ioctl]
    F -. wake_up .-> WQ[Wait queue]
  end
  subgraph USER[User space daemon]
    P[Producer thread<br/>ISource::read] --> Q1[(BlockingQueue Reading)]
    Q1 --> C[HvacController<br/>state machine]
    C -->|Hvac command| A[ISource::setHvac]
    C --> Q2[(BlockingQueue string)] --> L[Logger thread]
    L --> CSV[(CSV / stdout)]
  end
  DEV[/dev/smarthome/]
  F --- DEV
  P -- poll + read --> DEV
  A -- ioctl SET_HVAC --> DEV
```

## 2. Components and responsibilities
| Component | Responsibility |
|---|---|
| Timer + model (kernel) | Advance temperature, occupancy; generate a sample per tick |
| Ring buffer (kernel) | Decouple timer (producer) from reader; overwrite oldest when full |
| `file_operations` | User interface: `read`, `poll`, `unlocked_ioctl` |
| `ISource` | Hides whether data comes from driver or simulator |
| `DeviceSource` | RAII wrapper of fd; poll/read/ioctl |
| `SimSource` | Same physics in user space (testing, demo without root) |
| `HvacController` | Occupancy inference, comfort/eco band, hysteresis, fault handling |
| `BlockingQueue<T>` | Thread-safe hand-off with timeout and close |
| `Logger` | Asynchronous output so control never waits on disk |

## 3. Data structures
```c
struct sh_event  { u64 ts_ns; s32 temp_mc; u8 motion; u8 hvac; u16 reserved; };   // 16 B, no hidden padding
struct sh_status { s32 temp_mc, outdoor_mc, target_mc; u8 hvac, occupied; u16 reserved; u32 dropped; };
```
Ring buffer: free-running `head`/`tail` counters, index `% 64`; full when `head - tail == 64`
(unsigned wrap-around safe). Thermal model per tick (milli-C):
`delta = (outdoor - temp)/50 (+300 heat | -300 cool) (+20 if occupied)`.

Controller thresholds: occupied band `[target-h, target+h]`, vacant `[target-eco, target+eco]`;
heat stops at `lo + h`, cooling stops at `hi - h`.

## 4. UML

### Class diagram
```mermaid
classDiagram
  class ISource {
    <<interface>>
    +read(Reading&, int) bool
    +setHvac(Hvac)
    +setTarget(double)
  }
  class DeviceSource { -int fd_ ; +DeviceSource(path) ; +~DeviceSource() }
  class SimSource { -double temp_ ; -atomic~Hvac~ hvac_ ; -next() unsigned }
  class HvacController {
    -Config cfg_
    -CtrlState state_
    -bool occupied_
    +update(Reading) Hvac
    +state() CtrlState
    +occupied() bool
  }
  class Config { target_c ; hysteresis_c ; eco_offset_c ; vacancy_timeout_s ; fault_low_c ; fault_high_c }
  class Reading { ts_ns ; temp_c ; motion }
  class BlockingQueue~T~ { +push(T) ; +pop(T&, int) bool ; +close() }
  class Logger { -BlockingQueue~string~ q_ ; -thread worker_ ; +log(string) ; +stop() }
  ISource <|-- DeviceSource
  ISource <|-- SimSource
  HvacController o-- Config
  HvacController ..> Reading
  Logger *-- BlockingQueue
```

### Sequence diagram (one control cycle)
```mermaid
sequenceDiagram
  participant K as Kernel timer
  participant D as /dev/smarthome
  participant P as Producer thread
  participant Q as BlockingQueue
  participant C as Controller (main)
  participant L as Logger
  K->>D: model tick, push sh_event, wake_up
  P->>D: poll() returns POLLIN
  P->>D: read() -> sh_event
  P->>Q: push(Reading)
  C->>Q: pop(Reading)
  C->>C: update(): occupancy, band, state
  C->>D: ioctl(SET_HVAC, cmd)
  C->>L: log(CSV line)
  L->>L: write stdout + file
```

### State machine diagram
```mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Heating: temp < lo
  Idle --> Cooling: temp > hi
  Heating --> Idle: temp >= lo + h
  Cooling --> Idle: temp <= hi - h
  Idle --> Fault: temp out of 5..40 C
  Heating --> Fault: temp out of range
  Cooling --> Fault: temp out of range
  Fault --> Idle: temp within 6..39 C
```
`lo`/`hi` depend on occupancy (comfort vs. eco band).

## 5. Implementation plan
1. Driver skeleton: load/unload, device node. 2. Ring buffer + timer. 3. read/poll. 4. ioctl.
5. Controller + unit tests. 6. DeviceSource/SimSource. 7. Threads/queue/logger. 8. Integration,
sanitizers, docs.

## 6. Development environment
Ubuntu 22.04/24.04 (VM recommended for kernel work), `build-essential`, `linux-headers-$(uname -r)`,
g++ >= 9 (C++17), `make`, `git`, `strace`, `gdb`, optional `valgrind`. Editor: VS Code / vim.

## 7. Git repository & branching
`main` (stable, tagged `stage-N`, `v1.0`) <- `develop` <- `feature/driver`, `feature/controller`,
`feature/logger`, `docs/report`. Conventional commits (`feat:`, `fix:`, `test:`, `docs:`);
merge feature -> develop via pull request; run `scripts/git_setup.sh` to create it.

## 8. Documentation & progress tracking
Docs in `docs/`, progress log in `docs/stage4_prototype.md`, one GitHub Issue per requirement ID,
a Project board (To do / Doing / Done).
