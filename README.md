# Leveled Scheduler

This scheduler uses a leveled approach to scheduling tasks. Much like the Apple efficiency cores, we set aside the highest performing machines in one list, and the lowest performing machines in another list, based on MIPS. Then, we assign tasks between these two based on priority.

## Core Methods

- **Init()**: Assigns a set number of machines to S3 initially and defines performance split.

- **NewTask()**: Greedily assigns tasks to machines based on SLA.

