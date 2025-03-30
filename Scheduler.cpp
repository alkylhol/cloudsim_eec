//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
// greedy

#include "Scheduler.hpp"
#include <bits/stdc++.h>
#include <unordered_map>

typedef struct {
    MachineId_t id;
    MachineState_t s;
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
static unordered_map<MachineId_t, tasks_and_memory> pending;


static vector<TaskId_t> high_pri;
static vector<TaskId_t> mid_pri;
static vector<TaskId_t> low_pri;

static int tasks_completed;
// static int tasks_added;
static bool migrating = false;
//static unsigned active_machines = 0;
static machine_cpus mc;
static int migrate_frequency = 500;
static int cycle = 0;

// void TurnOnFraction(float frac, vector<MachineVMs> arr){
//     size_t i = 0;
//     while(i*1.0f/arr.size() < frac){
//         Machine_SetState(arr[i].id, S0);
//         arr[i].s = S0;
//         i++;
//         // machines.push_back(i);
//         // active_machines++;
//     }
//     while(i < arr.size()){
//         MachineState_t state_sleep = S3;
//         arr[i].s = S3;
//         Machine_SetState(arr[i].id, state_sleep);
//         i++;
//     }
// }     
static size_t arm_cnt = 0;
static size_t x86_cnt = 0;
static size_t riscv_cnt = 0;
static size_t power_cnt = 0;
void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    // 
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);
    //check how many machines of each cpu type
    size_t i;
    for (i = 0; i < Machine_GetTotal(); i++) {
        MachineVMs machine;// = {MachineId_t(i), {}};
        machine.id = MachineId_t(i);
        machine.s = S0;
        MachineState_t state_sleep = S3;
        size_t machine_cnt = 1000;
        switch(Machine_GetCPUType(MachineId_t(i))){
            case ARM:
                if(arm_cnt > machine_cnt){
                    machine.s = state_sleep;
                    Machine_SetState(machine.id, state_sleep);
                }
                arm_cnt++;
                mc.arm.push_back(machine);
                break;
            case X86:
                if(x86_cnt > machine_cnt){
                    machine.s = state_sleep;
                    Machine_SetState(machine.id, state_sleep);
                }
                x86_cnt++;
                mc.x86.push_back(machine);
                break;
            case RISCV:
                if(riscv_cnt > machine_cnt){
                    machine.s = state_sleep;
                    Machine_SetState(machine.id, state_sleep);
                }
                riscv_cnt++;
                mc.riscv.push_back(machine);
                break;
            case POWER:
                if(power_cnt > machine_cnt){
                    machine.s = state_sleep;
                    Machine_SetState(machine.id, state_sleep);
                }
                power_cnt++;
                mc.power.push_back(machine); 
                break;
        }
    }


    // TurnOnFraction(frac, mc.arm);
    // TurnOnFraction(frac, mc.x86);
    // TurnOnFraction(frac, mc.riscv); 
    // TurnOnFraction(frac, mc.power);
    

    //SimOutput("Scheduler::Init(): VM ids are " + to_string() + " ahd " + to_string(vms[1]), 3);
}




void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
    MachineId_t m = VM_GetInfo(vm_id).machine_id;
    MachineInfo_t m_info = Machine_GetInfo(m);
    vector<MachineVMs>* compat_machines;
    
    switch (m_info.cpu) {
        case ARM:
            compat_machines = &mc.arm;
            break;
        case X86:
            compat_machines = &mc.x86;
            break;
        case RISCV:
            compat_machines = &mc.riscv;
            break;
        case POWER:
            compat_machines = &mc.power;
            break;
    }
    size_t i = 0;
    for(i = 0; i < compat_machines->size(); i++){
        if((*compat_machines)[i].id == m){
            break;
        }
    }
    (*compat_machines)[i].vms.push_back(vm_id);

}

bool dec_comp (MachineVMs a, MachineVMs b) {
    MachineInfo_t a_info = Machine_GetInfo(a.id);
    MachineInfo_t b_info = Machine_GetInfo(b.id);
    float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
    float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
    return util_a > util_b;
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

bool Scheduler::FindMachine(TaskId_t task_id, bool active) {
    TaskInfo_t task = GetTaskInfo(task_id);
    vector<MachineVMs>* compat_machines = nullptr;

    switch(task.required_cpu){
        case ARM:
            compat_machines = &mc.arm;
            break;
        case X86:
            compat_machines = &mc.x86;
            break;
        case RISCV:
            compat_machines = &mc.riscv;
            break;
        case POWER:
            compat_machines = &mc.power;
            break;
    }

    if (!compat_machines) return false;

    size_t i = 0;
    sort(compat_machines->begin(), compat_machines->end(), energy_comp);
    
    for (i = 0; i < compat_machines->size(); i++) {
        MachineVMs& machine = (*compat_machines)[i];  // cleaner access
        MachineInfo_t m_info = Machine_GetInfo(machine.id);

        SimOutput("FindMachine " + to_string(m_info.machine_id) + " in state " + to_string(m_info.s_state), 4); 
        SimOutput("FindMachine transitioning to " + to_string((size_t)machine.s), 4); 

        if (active && (m_info.s_state != S0 || m_info.s_state != machine.s)) {
            continue;
        }
        if (!active && (m_info.s_state == S0 && m_info.s_state == machine.s)) {
            continue;
        }

        if (m_info.memory_used + task.required_memory < m_info.memory_size && m_info.active_tasks < m_info.num_cpus) {
            bool allowed = active;
            allowed = allowed || (!active && pending.find(machine.id) == pending.end());
            allowed = allowed || (!active 
                && pending.find(machine.id) != pending.end() 
                && pending[machine.id].tasks.size() < m_info.num_cpus 
                && pending[machine.id].memory_used + task.required_memory < m_info.memory_size);

            if (!allowed) continue;

            if (!active) {
                if (pending.find(machine.id) != pending.end()) {
                    pending[machine.id].tasks.push_back(task_id);
                    pending[machine.id].memory_used += task.required_memory;
                } else {
                    vector<TaskId_t> this_task = {task_id};
                    tasks_and_memory tandm;
                    tandm.tasks = this_task;
                    tandm.memory_used = task.required_memory;
                    pending[machine.id] = tandm;
                }

                if (machine.s != S0 && m_info.s_state == machine.s) {
                    machine.s = S0;
                    Machine_SetState(machine.id, S0);
                }
            } else {
                size_t j = 0;
                for (j = 0; j < machine.vms.size(); j++) {
                    if (VM_GetInfo(machine.vms[j]).vm_type == task.required_vm) {
                        SimOutput("Adding to ID " + to_string(machine.id), 4); 
                        VM_AddTask(machine.vms[j], task_id, task.priority);
                        return true;
                    }
                }

                if (j == machine.vms.size()) {
                    machine.vms.push_back(VM_Create(task.required_vm, task.required_cpu));
                    SimOutput("Adding to ID " + to_string(machine.id), 4); 
                    VM_Attach(machine.vms[j], machine.id);
                    VM_AddTask(machine.vms[j], task_id, task.priority);
                }
            }

            return true;
        }
    }   

    return false;
}

void Scheduler::AssignTasks(){
    bool done = false;
    while(!done && !high_pri.empty()){
        if(!FindMachine(high_pri[0], true)){
            if (!FindMachine(high_pri[0], false)){
                done = true;
                break;
            }
        }
        high_pri.erase(high_pri.begin());
    }
    while(!done && !mid_pri.empty()){
        if(!FindMachine(mid_pri[0], true)){
            if (!FindMachine(mid_pri[0], false)){
                done = true;
                break;
            }
        }
        mid_pri.erase(mid_pri.begin());
    }
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
    AssignTasks();


    // if(!FindMachine(task_id, true)){
    //     if (!FindMachine(task_id, false)){
    //         SimOutput("task not added", 0);
    //         switch(task.priority){
    //             case HIGH_PRIORITY:
    //                 high_pri.push_back(task_id);
    //                 break;
    //             case MID_PRIORITY:
    //                 mid_pri.push_back(task_id);
    //                 break;
    //             case LOW_PRIORITY:
    //                 low_pri.push_back(task_id);
    //                 break;
    //         }
    //     }
    // }
    // if(migrating) {
    //     VM_AddTask(vms[0], task_id, priority);
    // }
    // else {
    //     VM_AddTask(vms[task_id % active_machines], task_id, priority);
    // }// Skeleton code, you need to change it according to your algorithm
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    if((!high_pri.empty() || !mid_pri.empty() || !low_pri.empty())){
        SimOutput("Still some left", 4 );
    }
    AssignTasks();

}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    for(MachineVMs mvm: mc.arm) {
        for(auto & vm: mvm.vms) {
            VM_Shutdown(vm);
        }
    }
    for(MachineVMs mvm: mc.x86) {
        for(auto & vm: mvm.vms) {
            VM_Shutdown(vm);
        }
    }
    for(MachineVMs mvm: mc.riscv) {
        for(auto & vm: mvm.vms) {
            VM_Shutdown(vm);
        }
    }
    for(MachineVMs mvm: mc.power) {
        for(auto & vm: mvm.vms) {
            VM_Shutdown(vm);
        }
    }
    SimOutput("SimulationComplete(): Finished!", 0);
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
    TaskInfo_t task = GetTaskInfo(task_id);
    vector<MachineVMs>* compat_machines;
    
    switch (task.required_cpu) {
        case ARM:
            compat_machines = &mc.arm;
            break;
        case X86:
            compat_machines = &mc.x86;
            break;
        case RISCV:
            compat_machines = &mc.riscv;
            break;
        case POWER:
            compat_machines = &mc.power;
            break;
    }
    //idea: migration is expensive. Only do migration every frequency tasks
    cycle++;
    if(cycle == migrate_frequency){
        cycle = 0;
        size_t i = 0;
        TaskId_t dummy = 4294967295;
        size_t min_task_sum = dummy;
        VMId_t min_vm = dummy;
        MachineId_t min_mac = dummy;
        // sort the actual vector via the pointer
        sort(compat_machines->begin(), compat_machines->end(), comp);
        
        for (i = 0; i < compat_machines->size() / 2; i++) {
            MachineVMs& mvm = (*compat_machines)[i];
            if (Machine_GetInfo(mvm.id).memory_used > 0 && Machine_GetInfo(mvm.id).s_state == S0 && mvm.s == S0) {
                for (VMId_t vm : mvm.vms) {
                    size_t task_sum = 0;
                    for (TaskId_t t : VM_GetInfo(vm).active_tasks) {
                        task_sum += GetTaskInfo(t).required_memory;
                    }
                    if(min_vm == dummy || task_sum > min_task_sum){
                        min_vm = vm;
                        min_task_sum = task_sum;
                        min_mac = mvm.id;
                    } 
                }
            }
        }
        if(min_vm == dummy){
            return;
        }
        VMInfo_t min_vm_info = VM_GetInfo(min_vm);
        // we now have the smallest vm in the right half.
        // find if any vms in the left half can support this vm.
        bool found_vm;
        for (i = compat_machines->size() - 1; i >= compat_machines->size() / 2; i--) {
            MachineVMs& mvm = (*compat_machines)[i];
            MachineInfo_t mvm_info = Machine_GetInfo(mvm.id);
            if (mvm_info.memory_used > 0 && mvm_info.s_state == S0 && mvm.s == S0) {
                bool can_fit = (mvm_info.memory_used + min_task_sum < mvm_info.memory_size);
                can_fit = can_fit && (min_vm_info.active_tasks.size() + mvm_info.active_tasks < mvm_info.num_cpus);
                if(mvm_info.memory_used + min_task_sum < mvm_info.memory_size){
                    // good vm
                    found_vm = true;
                    break;
                }
            }
        }
        if(found_vm && min_vm != dummy){
            size_t i = 0;
            for(i = 0; i < compat_machines->size(); i++){
                if((*compat_machines)[i].id == min_mac){
                    break;
                }
            }
            if(i == compat_machines->size()){
                SimOutput("w", 0);
            }
            size_t j = 0;
            bool found = false;
            for(j = 0; j < (*compat_machines)[i].vms.size(); j++){
                if((*compat_machines)[i].vms[j] == min_vm){
                    found = true;
                    (*compat_machines)[i].vms.erase((*compat_machines)[i].vms.begin() + j);
                    break;
                }
            }
            if(!found){
                SimOutput("fdfsd", 0);
            }

            VM_Migrate(min_vm, (*compat_machines)[i].id);
        }
    }
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete", 4);
    tasks_completed++;
    SimOutput("Scheduler::TaskComplete(): " + to_string(tasks_completed) + " tasks completed ", 4);
}


void Scheduler::ChangeComplete(Time_t now, MachineId_t machine_id){
    MachineInfo_t m_info = Machine_GetInfo(machine_id);
    SimOutput("StateChangeComplete: Machine "+ to_string(m_info.machine_id) + " in state " + to_string(m_info.s_state) + " ", 4); 
    SimOutput("StateChangeComplete: Machine has "+ to_string(m_info.active_tasks) + " tasks ", 4); 
    vector<MachineVMs>* compat_machines = nullptr;
    switch (m_info.cpu) {
        case ARM:
            compat_machines = &mc.arm;
            break;
        case X86:
            compat_machines = &mc.x86;
            break;
        case RISCV:
            compat_machines = &mc.riscv;
            break;
        case POWER:
            compat_machines = &mc.power;
            break;
    }
    size_t i = 0;
    for(i = 0; i < compat_machines->size(); i++){
        if((*compat_machines)[i].id == machine_id){
            break;
        }
    }
    (*compat_machines)[i].s = m_info.s_state;
    if (m_info.s_state != S0 && pending.find(machine_id) != pending.end()) {
        (*compat_machines)[i].s = S0;
        Machine_SetState(machine_id, S0);
    } else if(m_info.s_state == S0){
        if(i == compat_machines->size()){
            (*compat_machines)[i].id = machine_id;
        }

        //exists
        vector<TaskId_t>& tasks = pending[machine_id].tasks;
        // SimOutput("StateChangeComplete: " + to_string(tasks_added) + " tasks added ", 0);
        while (!tasks.empty()){
            TaskInfo_t task = GetTaskInfo(tasks[0]);
            // m_info.active_tasks --;
            // m_info.memory_used -= task.required_memory;
            size_t j = 0;
            for(j = 0; j < (*compat_machines)[i].vms.size(); j++){
                if(VM_GetInfo((*compat_machines)[i].vms[j]).vm_type == task.required_vm){
                    // tasks_added++;
                    // SimOutput("StateChangeComplete: " + to_string(tasks_added) + " tasks added ", 0)
                    VM_AddTask((*compat_machines)[i].vms[j], tasks[0], task.priority);
                    break;
                }
            }
            if(j == (*compat_machines)[i].vms.size()){                
                (*compat_machines)[i].vms.push_back(VM_Create(task.required_vm, task.required_cpu));
                //SimOutput("That one", 0);
                VM_Attach((*compat_machines)[i].vms[j], (*compat_machines)[i].id);
                // tasks_added++;
                // SimOutput("StateChangeComplete: " + to_string(tasks_added) + " tasks added ", 0);
                VM_AddTask((*compat_machines)[i].vms[j], tasks[0], task.priority);
            }
            tasks.erase(tasks.begin());
        }
        pending.erase(machine_id);
    }
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
    Scheduler.ChangeComplete(time, machine_id);
}

