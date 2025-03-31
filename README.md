# E-Eco Scheduler

This scheduler uses a e-eco approach to scheduling tasks, as defined in lecture. We 
put a specific fraction of machines in S0, S3, and S5. Then, we manage the number
of machines in each bin, ensuring that each level stays close to its ideal fraction.

## Core Methods

- **Init()**: Assigns a set fraction of machines to S0, S3, and S5.

- **NewTask()**: Greedily assigns tasks to S0 machines. If none are available,
we look through S3, and then S5 machines and wake them up.

- **TaskComplete()**: Finds the lowest task on the lower utilization half of machines and migrates it to an available higher utilization machine.
