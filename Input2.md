# High performing GPU machine
machine class:
{
        Number of machines: 16
        CPU type: X86
        Number of cores: 8
        Memory: 65536
        S-States: [600, 400, 300, 200, 100, 50, 0]
        P-States: [100, 60, 30, 12]
        C-States: [120, 60, 40, 10]
        MIPS: [8000, 4000, 1000, 600]
        GPUs: yes
}
# This task is very intensive AI model training. Hogs memory, but is SLA3. 
# This is best effort, so we should not get memory overflows.
task class:
{
        Start time: 1000
        End time : 1500
        Inter arrival: 10
        Expected runtime: 2000000
        Memory: 16384
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA3
        CPU type: X86
        Task type: AI
        Seed: 520230
}