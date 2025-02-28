# Low performing efficient machine
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [100, 75, 50, 20, 5, 1, 0]
        P-States: [12, 6, 3, 2]
        C-States: [12, 2, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: no
}

# Redundant wasteful machine, scheduler should only use if util is too high.
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [600, 400, 300, 200, 100, 50, 0]
        P-States: [100, 60, 30, 12]
        C-States: [120, 60, 40, 10]
        MIPS: [1000, 300, 200, 100]
        GPUs: no
}

# High performing GPU machine
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 65536
        S-States: [600, 400, 300, 200, 50, 10, 0]
        P-States: [120, 60, 30, 20]
        C-States: [120, 20, 10, 0]
        MIPS: [1500, 1000, 700, 500]
        GPUs: yes
}

# POWER machine
machine class:
{
        Number of machines: 16
        CPU type: POWER
        Number of cores: 8
        Memory: 16384
        S-States: [100, 75, 50, 20, 5, 1, 0]
        P-States: [12, 6, 3, 2]
        C-States: [12, 2, 1, 0]
        MIPS: [1000, 800, 600, 400]
        GPUs: no
}

# Can our system handle a person spamming web requests?
task class:
{
        Start time: 1000
        End time : 4000
        Inter arrival: 3
        Expected runtime: 1000
        Memory: 8
        VM type: LINUX
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}

# This task is high-memory AI training. Hogs memory, but is SLA3. 
# SLA3 is best effort, so we should not get memory overflows.
task class:
{
        Start time: 6000
        End time : 7500
        Inter arrival: 10
        Expected runtime: 50000
        Memory: 16384
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA3
        CPU type: X86
        Task type: AI
        Seed: 520230
}

# AIX web requests. This tests the scheduler's ability to find compatible machines
task class:
{
        Start time: 1000
        End time : 1500
        Inter arrival: 100
        Expected runtime: 15000
        Memory: 1024
        VM type: AIX
        GPU enabled: no
        SLA type: SLA0
        CPU type: POWER
        Task type: WEB
        Seed: 520230
}

# Very sparse GPU tasks. Ideally we don't use GPU machine when we don't have a
# GPU task by setting a different S-state.
task class:
{
        Start time: 2000000
        End time : 4000000
        Inter arrival: 150000
        Expected runtime: 15000
        Memory: 1024
        VM type: WIN
        GPU enabled: yes
        SLA type: SLA3
        CPU type: X86
        Task type: CRYPTO
        Seed: 520230
}

# Compute-intensive non-GPU tasks. Test if scheduler knows not to use wasteful
# machine or GPU machine.
task class:
{
        Start time: 2000000
        End time : 2001000
        Inter arrival: 100
        Expected runtime: 10000
        Memory: 512
        VM type: WIN
        GPU enabled: no
        SLA type: SLA2
        CPU type: X86
        Task type: STREAM
        Seed: 520230
}
