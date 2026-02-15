# Command Execution Units (CEU) Documentation

## Team Members

- @Nikhil-771  
- @Amarworks  

---

## Definition

In PicoKernel, a **Command Execution Unit (CEU)** is a statically compiled kernel component that implements the execution logic associated with a specific command.

Each command registered in the system maps to a corresponding CEU responsible for performing its operational logic under kernel control.

---

## Architectural Context

User (CLI)
   ↓
Interface Layer
   ↓
Kernel Core (Routing)
   ↓
Command Execution Unit (CEU)
   ↓
Driver / Protocol Subsystem
   ↓
Hardware (RP2350 / ESP32)

CEUs executes within kernel space and are invoked through the kernel routing mechanism.

---

## Embedded Design Characteristics

Since PicoKernel targets RP2350 (Pico 2W), all CEUs are:

- statically linked at compile time  
- resident in memory at boot  
- invoked synchronously  
- required to complete within bounded time  

---

## CEU Responsibilities

Each CEU:

- implements the logic of a specific command  
- performs bounded and deterministic execution  
- processes validated input from the kernel  
- interacts with lower subsystems through defined kernel interfaces  
- returns structured results to the caller  

---

## Execution Flow

When a command is issued:

1. The Interface layer parses input.
2. The Kernel resolves the command identifier.
3. The mapped CEU is invoked.
4. The CEU executes its logic.
5. Results are returned through the kernel.

---

## Structure

Each CEU resides in its own directory:

modules/
└── <ceu_name>/
    ├── <ceu_name>.h
    └── <ceu_name>.c

The header file defines the CEU interface.  
The source file contains the implementation.

---

## Registration Model

CEUs are registered at boot through a compile-time registry table.

The registry maps command identifiers to CEU function pointers and enables deterministic routing.

---

## Future Plans  

### Part 1: Development Plan (Starting Soon...)

CEU development will proceed through the following small, isolated projects before full PicoKernel integration.

#### Project 1 – Minimal CEU Skeleton  

Establish CEU structure with explicit input/output interfaces and strict execution boundaries.

#### Project 2 – CEU Registration Model  

Implement a compile-time command-to-CEU registry with deterministic routing.

#### Project 3 – Kernel Interaction Discipline  

Enforce kernel-API-only interaction and maintain architectural boundary integrity.

#### Project 4 – Bounded Execution Enforcement  

Implement execution guards to ensure CEUs complete within defined time constraints.

#### Project 5 – Protocol Delegation Model  

Design and implement controlled delegation of heavy operations through protocol interfaces.

#### Project 6 – Structured Error Handling  

Standardize CEU return types, result codes, and error propagation mechanisms.

#### Project 7 – Defensive Modules Design  

Apply strict input validation, fixed-buffer discipline, and secure coding practices.

#### Project 8 – PicoKernel Modules Integration  

Integrate validated CEUs into the PicoKernel routing core and driver subsystem.

### Modules List(Part 2)

#### Phase 1 – Core modules

uptime
sysinfo
buildinfo
version
logging
stats
paniclog
command_registry
help

#### Phase 2 – Extended modules

eventbus
netproxy
telemetry

#### Phase 3 – ESP32-Backed modules(later on...)

wifi
bluetooth
network
time
date
webserver
