# Greedy Cloud Scheduler

A C++ scheduler module for a cloud-simulation harness. It prioritizes tasks by SLA and GPU capability, places
them on compatible machines, wakes sleeping capacity when necessary, and periodically consolidates work through
VM migration.

## Scheduling strategy

- Group machines by CPU architecture (`ARM`, `X86`, `RISCV`, or `POWER`).
- Queue new tasks by SLA priority and GPU capability.
- Prefer active compatible machines with higher memory utilization so work is consolidated.
- Reuse a compatible VM when possible, or create and attach one for the required VM type.
- Transition sleeping machines to `S0` when active capacity cannot accept a task.
- Periodically evaluate lower-utilization machines and migrate a VM when a more utilized machine has capacity.

The simulator models machine states from active `S0` through progressively deeper sleep states to powered-down
`S5`. The scheduler initially places excess machines in `S3` and wakes them on demand.

## Integration

This repository contains the scheduler and the simulator-facing interface headers, not a standalone simulator
executable. Integrate `Scheduler.cpp`, `Scheduler.hpp`, and the provided interface headers with the compatible
course simulation harness, then select the `Greedy` branch implementation.

## Core methods

- `Init()` initializes CPU-specific machine pools and sleep states.
- `NewTask()` classifies and queues tasks, then attempts greedy placement.
- `FindMachine()` selects compatible capacity and handles machine wake-up.
- `TaskComplete()` periodically evaluates whether a VM can be migrated to consolidate utilization.
- `ChangeComplete()` attaches queued work after a machine reaches its requested state.

## My contributions

I implemented and iterated on the greedy scheduler in `Scheduler.cpp`, including SLA/GPU-aware task queues,
machine selection and state transitions, priority fixes, and VM migration support. The simulator interfaces and
other repository work were contributed by teammates.
