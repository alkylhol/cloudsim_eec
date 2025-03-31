# pMapper Scheduler

This scheduler uses a modified pMapper scheduler. We define the pMapper fit score by a combination of memory utilization, cpu utilization, MIPS per Watt, and S state.

## Core Methods

- **Init()**: Sorts machines by fit score initially, turning on the best fit score machines.

- **NewTask()**: Sorts machines by fit score and finds the first available machine. If no awake machines are available, wakes up some machines.

- **TaskComplete()**: Finds the lowest task on the lower utilization half of machines and migrates it to an available higher utilization machine.
