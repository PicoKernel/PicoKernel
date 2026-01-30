# Modules Documentation

## Team Members
- @Nikhil-771
- @Amarworks

---

## Definition of Modules

Modules are the **features of the operating system**. They implement OS-level services and expose functionality to the Interface layer through stable APIs.

---

## Our Implementation of Modules within PicoKernel

Since PicoKernel targets embedded hardware (RP2350 Pico 2W), the module model differs from traditional operating systems.

Because the hardware lacks:
- MMU
- kernel-user separation
- virtual memory


All modules are:
- statically compiled
- always present
- executed synchronously
- bounded in execution time

No module is dynamically loaded, unloaded, or replaced at runtime.

---

## Module Responsibilities

The Modules layer is responsible for:
- implementing OS services
- providing stable programs
- performing bounded logical work
- returning structured results for a called function

---

## Layer Model

```
User (CLI)
   ↓
Interface
   ↓
Kernel (API)
   ↓
Modules
   ↓
Kernel (APIs only)
   ↓
Drivers
   ↓
Hardware (RP2350 / ESP32)
```

The kernel enforces all layer boundaries.

---

## Execution Model

Modules are invoked synchronously by the Interface layer or kernel routing logic.

Rules:
- modules must return
- modules must not block or sleep
- modules must complete in bounded time

Long-running work is delegated through protocol APIs (ESP32 integration (later part of the project)).

---

## Module Structure

Each module resides in its own directory under `modules/`:

```
modules/
└── <module_name>/
    ├── <module_name>.h
    └── <module_name>.c
```

The header defines the public API.  
The source file contains the implementation.

---

## Module Registration

Modules are registered at boot via a static registry table.

Registration is data-driven, not code-driven. This ensures:
- scalability
- no kernel modification per module
- predictable routing

---
## Future plans 
### Part 1: Development Plan(Starting soon...)

Module development will proceed through the following small isolated projects before integration.

#### Project 1 — Minimal Module Skeleton
Establish module structure and API separation.

#### Project 2 — Module Registration Model
Implement data-based module registration.

#### Project 3 — Kernel Interaction Discipline
Enforce API-only kernel interaction.

#### Project 4 — Bounded Execution
Ensure modules cannot stall the system.

#### Project 5 — Protocol Delegation
Delegate heavy work via protocol APIs.

#### Project 6 — Error Handling
Standardize error reporting and logging.

#### Project 7 — Secure Module Design
Apply defensive programming rules.

#### Project 8 — Transition to PicoKernel Modules
Move from scaffolding to real OS modules.

---

### Part 2: Module List(Tailored for PicoKernel integration...)

#### Phase 1 — Core OS Modules
```
uptime
sysinfo
buildinfo
version
logging
stats
paniclog
command_registry
help
```

#### Phase 2 — Extended OS Modules
```
eventbus
netproxy
telemetry
```

#### Phase 3 — Network / Time Modules (ESP32-backed)
```
wifi
bluetooth
network
ntp
time
date
webserver
```

This concludes our Team's current plans and goals, as we build and learn, we will keep this README updated! See ya!
 