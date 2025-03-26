//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//
// this is pmapper
#include "Scheduler.hpp"
#include <bits/stdc++.h>
#include <unordered_map>

typedef struct {
    MachineId_t id;
    vector<VMId_t> vms;
} MachineVMs;

typedef struct {
    vector<MachineVMs> arm;
    vector<MachineVMs> power;
    vector<MachineVMs> riscv;
    vector<MachineVMs> x86;
} machine_cpus;
typedef struct {
    vector<TaskId_t> tasks;
    size_t memory_used;
} tasks_and_memory;
unordered_map<MachineId_t, tasks_and_memory> pending;


vector<TaskId_t> high_pri;
vector<TaskId_t> mid_pri;
vector<TaskId_t> low_pri;


static bool migrating = false;
//static unsigned active_machines = 0;
static machine_cpus mc;

void TurnOnFraction(float frac, vector<MachineVMs> arr){
    size_t i = 0;
    while(i*1.0f/arr.size() < frac){
        Machine_SetState(arr[i].id, S0);
        i++;
        // machines.push_back(i);
        // active_machines++;
    }
    while(i < arr.size()){
        Machine_SetState(arr[i].id, S0);
        i++;
    }
}

float fit_score(MachineInfo_t m_info){
    float mem_util = (float)m_info.memory_used / (float)m_info.memory_size;
    float cpu_util = (float)m_info.active_tasks / (float)m_info.num_cpus;

    float power_penalty = (m_info.s_state == S0) ? 0.0f : 0.2f * m_info.s_state;

    float mips_per_p = (float)m_info.performance[0] / (float)m_info.p_states[0];
    return mips_per_p + mem_util + cpu_util - power_penalty;
}
bool energy_comp(MachineVMs a, MachineVMs b) {
    //return false;
    //return Machine_GetInfo(a.id).s_states[0] < Machine_GetInfo(b.id).s_states[0];
    MachineInfo_t am = Machine_GetInfo(a.id);
    MachineInfo_t bm = Machine_GetInfo(b.id);
    return fit_score(am) > fit_score(bm);
}

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
     
    //      Get if there is a GPU or not
    // 
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    //check how many machines of each cpu type
    size_t i;
    for (i = 0; i < Machine_GetTotal(); i++) {
        MachineVMs machine;// = {MachineId_t(i), {}};
        machine.id = MachineId_t(i);

        switch(Machine_GetCPUType(MachineId_t(i))){
            case ARM:
                mc.arm.push_back(machine);
                break;
            case X86:
                mc.x86.push_back(machine);
                break;
            case RISCV:
                mc.riscv.push_back(machine);
                break;
            case POWER:
                mc.power.push_back(machine); 
                break;
        }
    }
    sort(mc.arm.begin(), mc.arm.end(), energy_comp);
    sort(mc.x86.begin(), mc.x86.end(), energy_comp);
    sort(mc.riscv.begin(), mc.riscv.end(), energy_comp);
    sort(mc.power.begin(), mc.power.end(), energy_comp);

    //turn on about 1/3 of the machines
    float frac = 1.0f/3.0f;
    TurnOnFraction(frac, mc.arm);
    TurnOnFraction(frac, mc.x86);
    TurnOnFraction(frac, mc.riscv); 
    TurnOnFraction(frac, mc.power);
    

    //SimOutput("Scheduler::Init(): VM ids are " + to_string() + " ahd " + to_string(vms[1]), 3);
}




void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    
}



bool Scheduler::FindMachine(TaskId_t task_id, bool active) {
    TaskInfo_t task = GetTaskInfo(task_id);
    vector<MachineVMs> compat_machines;
    switch(task.required_cpu){
        case ARM:
            compat_machines = mc.arm;
            break;
        case X86:
            compat_machines = mc.x86;
            break;
        case RISCV:
            compat_machines = mc.riscv;
            break;
        case POWER:
            compat_machines = mc.power;
            break;
    }

    size_t i = 0;
    sort(compat_machines.begin(), compat_machines.end(), energy_comp);
    for(i = 0; i < compat_machines.size(); i++) {
        //MachineVMs machine = compat_machines[i];
        MachineInfo_t m_info = Machine_GetInfo(compat_machines[i].id);
        if (active && m_info.s_state != S0) {
            continue;
        }
        if (!active && m_info.s_state == S0) {
            continue;
        }
        if (m_info.memory_used + task.required_memory < m_info.memory_size && m_info.active_tasks < m_info.num_cpus) {
            bool allowed = active;
            allowed = allowed || (!active && pending.find(compat_machines[i].id) == pending.end());
            allowed = allowed || (!active 
                    && pending.find(compat_machines[i].id) != pending.end() 
                    && pending[compat_machines[i].id].tasks.size() < m_info.num_cpus 
                    && pending[compat_machines[i].id].memory_used + task.required_memory < m_info.memory_size);
            if(!allowed) continue;
            size_t j = 0;
            for(j = 0; j < compat_machines[i].vms.size(); j++){
                if(VM_GetInfo(compat_machines[i].vms[j]).vm_type == task.required_vm){
                    VM_AddTask(compat_machines[i].vms[j], task_id, task.priority);
                    return true;
                }
            }
            if(j == compat_machines[i].vms.size()){

                if(!active){
                    if (pending.find(compat_machines[i].id) != pending.end()) {
                        //exists
                        pending[compat_machines[i].id].tasks.push_back(task_id);
                        pending[compat_machines[i].id].memory_used += task.required_memory;
                    } else {
                        vector<TaskId_t> this_task;
                        this_task.push_back(task_id);
                        tasks_and_memory tandm;
                        tandm.tasks = this_task;
                        tandm.memory_used = task.required_memory;
                        pending[compat_machines[i].id] = tandm;
                    }
                    Machine_SetState(compat_machines[i].id, S0);
                    // m_info.memory_used += task.required_memory;
                    // m_info.active_tasks ++;
                    //return false;
                } else {
                    compat_machines[i].vms.push_back(VM_Create(task.required_vm, task.required_cpu));
                    //SimOutput("That one", 0);
                    VM_Attach(compat_machines[i].vms[j], compat_machines[i].id);
                    VM_AddTask(compat_machines[i].vms[j], task_id, task.priority);
                }
                // compat_machines[i].vms.push_back(VM_Create(task.required_vm, task.required_cpu));
                // VM_Attach(compat_machines[i].vms[j], compat_machines[i].id);
                // VM_AddTask(compat_machines[i].vms[j], task_id, task.priority);
            }
            return true;
        }
    }   
    return false;
}
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // Get the task parameters
    TaskInfo_t task = GetTaskInfo(task_id);
    switch(task.required_sla){
        case SLA0:
        case SLA1:
            high_pri.push_back(task_id);
            break;
        case SLA2:
            mid_pri.push_back(task_id);
            break;
        case SLA3:
            low_pri.push_back(task_id);
            break;
    }
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    bool done = false;
    while(!done && !high_pri.empty()){
        if(!FindMachine(high_pri[0], true)){
            if (!FindMachine(high_pri[0], false)){
                done = true;
                break;
            }
        }
        high_pri.erase(high_pri.begin());
        // if (FindMachine(high_pri[0], true) || FindMachine(high_pri[0], false))
    }
    if (done) return;
    while(!done && !mid_pri.empty()){
        if(!FindMachine(mid_pri[0], true)){
            if (!FindMachine(mid_pri[0], false)){
                done = true;
                break;
            }
        }
        mid_pri.erase(mid_pri.begin());
    }
    if (done) return;

    while(!done && !low_pri.empty()){
        if(!FindMachine(low_pri[0], true)){
            if (!FindMachine(low_pri[0], false)){
                done = true;
                break;
            }
        }
        low_pri.erase(low_pri.begin());
    }
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(auto & vm: vms) {
        VM_Shutdown(vm);
    }
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

bool comp (MachineVMs a, MachineVMs b) {
    MachineInfo_t a_info = Machine_GetInfo(a.id);
    MachineInfo_t b_info = Machine_GetInfo(b.id);
    float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
    float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
    return util_a < util_b;
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    //SimOutput("complete run", 0);
    TaskInfo_t task = GetTaskInfo(task_id);
    vector<MachineVMs> compat_machines;
    switch (task.required_cpu) {
        case ARM:
            compat_machines = mc.arm;
            break;
        case X86:
            compat_machines = mc.x86;
            break;
        case RISCV:
            compat_machines = mc.riscv;
            break;
        case POWER:
            compat_machines = mc.power;
            break;
    }
    size_t i = 0;
    TaskId_t dummy = 4294967295;
    TaskId_t min_task = dummy;
    sort(compat_machines.begin(), compat_machines.end(), comp);
    for(i = 0; i < compat_machines.size()/2; i++){
        if (Machine_GetInfo(compat_machines[i].id).memory_used > 0 && Machine_GetInfo(compat_machines[i].id).s_state == S0) {
            for(VMId_t vm : compat_machines[i].vms){
                for(TaskId_t t : VM_GetInfo(vm).active_tasks){
                    if(min_task == dummy){
                        min_task = t;
                    } else {
                        min_task = GetTaskInfo(min_task).required_memory < GetTaskInfo(t).required_memory ? min_task : t;
                    }
                }
            }
            if(min_task != dummy){
                //found a task
                break;
            }
        }
    }
    if(min_task != dummy){
        NewTask(now, min_task);
    }
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
}

// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
    migrating = false;
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    //SimOutput(to_string(time), 0);
    Scheduler.PeriodicCheck(time);

}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    //SimOutput(to_string(time), 0);
    MachineInfo_t m_info = Machine_GetInfo(machine_id);
    if(m_info.s_state == S0){
        if (pending.find(machine_id) != pending.end()) {
            //SimOutput("Here", 0);
            vector<MachineVMs> compat_machines;
            MachineVMs this_m;
            switch (m_info.cpu) {
                case ARM:
                    compat_machines = mc.arm;
                    break;
                case X86:
                    compat_machines = mc.x86;
                    break;
                case RISCV:
                    compat_machines = mc.riscv;
                    break;
                case POWER:
                    compat_machines = mc.power;
                    break;
            }
            size_t i = 0;
            for(i = 0; i < compat_machines.size(); i++){
                if(compat_machines[i].id == machine_id){
                    this_m = compat_machines[i];
                    break;
                }
            }
            if(i == compat_machines.size()){
                this_m.id = machine_id;
            }

            //exists
            vector<TaskId_t>& tasks = pending[machine_id].tasks;
            while (!tasks.empty()){
                TaskInfo_t task = GetTaskInfo(tasks[0]);
                // m_info.active_tasks --;
                // m_info.memory_used -= task.required_memory;
                size_t j = 0;
                for(j = 0; j < this_m.vms.size(); j++){
                    if(VM_GetInfo(this_m.vms[j]).vm_type == task.required_vm){
                        VM_AddTask(this_m.vms[j], tasks[0], task.priority);
                    }
                }
                if(j == this_m.vms.size()){                
                    this_m.vms.push_back(VM_Create(task.required_vm, task.required_cpu));
                    //SimOutput("That one", 0);
                    VM_Attach(this_m.vms[j], this_m.id);
                    VM_AddTask(this_m.vms[j], tasks[0], task.priority);
                }
                tasks.erase(tasks.begin());
            }
            pending.erase(machine_id);
        }
    }
}

