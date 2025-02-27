# This input file tests the scheduler's awareness to GPU needs and energy
# GPU machine, uses lots of energy
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [600, 400, 300, 200, 50, 10, 0]
        P-States: [120, 60, 30, 20]
        C-States: [120, 20, 10, 0]
        MIPS: [1500, 1000, 700, 500]
        GPUs: yes
}

# Non-GPU machine, efficient
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

# Non-GPU task. We should never assign these to the GPU machine.
task class:
{
        Start time: 1000
        End time : 2000000
        Inter arrival: 10000
        Expected runtime: 50000
        Memory: 128
        VM type: WIN
        GPU enabled: no
        SLA type: SLA0
        CPU type: X86
        Task type: WEB
        Seed: 520230
}

# Very sparse GPU tasks. Ideally we don't use GPU machine when we don't have a
# GPU task.
task class:
{
        Start time: 1000
        End time : 2000000
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

