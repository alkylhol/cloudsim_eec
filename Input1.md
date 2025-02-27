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
