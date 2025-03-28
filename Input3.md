# This input file tests the scheduler's ability to compatibly assign tasks to machines
# X86 machine
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

# Tasks for an X86 machine
task class:
{
        Start time: 1000
        End time : 1500
        Inter arrival: 100
        Expected runtime: 2000000
        Memory: 1024
        VM type: LINUX
        GPU enabled: yes
        SLA type: SLA0
        CPU type: X86
        Task type: AI
        Seed: 520230
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

# AIX web requests. 
task class:
{
        Start time: 1000
        End time : 1500
        Inter arrival: 100
        Expected runtime: 2000000
        Memory: 1024
        VM type: AIX
        GPU enabled: no
        SLA type: SLA0
        CPU type: POWER
        Task type: WEB
        Seed: 520230
}