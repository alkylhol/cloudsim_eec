# Greedy Scheduler

This scheduler uses a greedy approach to scheduling tasks.

## Core Methods

- **Init()**: Assigns a set number (`machine_count`) of machines to S3 initially.

- **NewTask()**: Greedily assigns tasks by sorting by decreasing utilization and finding the first available machine. If no awake machines are available, wakes up some machines.

- **TaskComplete()**: Finds the lowest task on the lower utilization half of machines and migrates it to an available higher utilization machine.
