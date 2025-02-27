# Wasteful machine
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

# Efficient machine
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 16384
        S-States: [100, 70, 50, 20, 10, 5, 0]
        P-States: [50, 10, 5, 3]
        C-States: [50, 8, 5, 0]
        MIPS: [800, 200, 100, 50]
        GPUs: no
}

# We start assigning tasks to the efficient machine, using wasteful only if 
# utilization is too high. Naive scheduler uses 0.17 kWh instead of 0.024 kWh,
# which we get when we turn wasteful off (delete it).

task class:
{
        Start time: 1000
        End time : 1500
        Inter arrival: 100
        Expected runtime: 2000000
        Memory: 1024
        VM type: WIN
        GPU enabled: yes
        SLA type: SLA2
        CPU type: X86
        Task type: HPC
        Seed: 520230
}