//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include "Scheduler.hpp"
#include <bits/stdc++.h>

static bool migrating = false;
static unsigned active_machines = 16;

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
    for(unsigned i = 0; i < active_machines; i++)
        vms.push_back(VM_Create(LINUX, X86));
    for(unsigned i = 0; i < active_machines; i++) {
        machines.push_back(MachineId_t(i));
    }    
    for(unsigned i = 0; i < active_machines; i++) {
        VM_Attach(vms[i], machines[i]);
    }

    bool dynamic = false;
    if(dynamic)
        for(unsigned i = 0; i<4 ; i++)
            for(unsigned j = 0; j < 8; j++)
                Machine_SetCorePerformance(MachineId_t(0), j, P3);
    // Turn off the ARM machines
    for(unsigned i = 24; i < Machine_GetTotal(); i++)
        Machine_SetState(MachineId_t(i), S5);

    SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " ahd " + to_string(vms[1]), 3);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    // Update your data structure. The VM now can receive new tasks
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // Get the task parameters
    TaskInfo_t task = GetTaskInfo(task_id);
    
    // Decide to attach the task to an existing VM, 
    //  vm.AddTask(taskid, Priority_T priority); or
    // Create a new VM, attach the VM to a machine
    //      VM vm(type of the VM)
    //      vm.Attach(machine_id);
    //      vm.AddTask(taskid, Priority_t priority) or
    // Turn on a machine, create a new VM, attach it to the VM, then add the task
    //
    // Turn on a machine, migrate an existing VM from a loaded machine....
    //
    // Other possibilities as desired
    size_t i = 0;
    for(i = 0; i < vms.size(); i++) {
        VMId_t vm = vms[i];
        VMInfo_t vm_info = VM_GetInfo(vm);
        if(task.required_cpu == vm_info.cpu && task.required_vm == vm_info.vm_type){
            MachineId_t m = vm_info.machine_id;
            MachineInfo_t m_info = Machine_GetInfo(m);
            if (m_info.memory_used + task.required_memory < m_info.memory_size) {
                break;
            }
        }
    }
    if (i == vms.size()) {
        //turn on new machine
        vms.push_back(VM_Create(task.required_vm, task.required_cpu));
        MachineId_t
        machines.push_back( MachineId_t(i));
        VM_Attach(vms[i], machines[i]);
        active_machines++;
    } else {
        VM_AddTask(vms[i], task_id, GetTaskInfo(task_id).priority);
    }
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
    // for(size_t i = 0; i < vms.size(); i++) {
    //         VMInfo_t vm_info = VM_GetInfo(vms[i]);
    //         MachineInfo_t m_info = Machine_GetInfo(vm_info.machine_id);
    //         if (m_info.memory_used == 0) {
    //             VM_Shutdown(vms[i]);
    //             vms.erase(vms.begin() + i);
    //             machines.erase(machines.begin() + i);
    //             active_machines--;
    //         }
    // }
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

bool comp (VMId_t a, VMId_t b) {
    MachineInfo_t a_info = Machine_GetInfo(VM_GetInfo(a).machine_id);
    MachineInfo_t b_info = Machine_GetInfo(VM_GetInfo(b).machine_id);
    float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
    float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
    return util_a < util_b;
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    size_t i = 0;
    for(i = 0; i < vms.size(); i++){
        VMInfo_t vm = VM_GetInfo(vms[i]);

        if(count(vm.active_tasks.begin(), vm.active_tasks.end(), task_id) > 0){
            break;
        } 
    }
    if(i == vms.size()){
        //error bad
    } else {
        VM_RemoveTask(vms[i], task_id);
    }
    sort(vms.begin(), vms.end(), comp);
    for(i = 0; i < vms.size(); i++){
        machines[i] = vms[VM_GetInfo(vms[i]).machine_id];
    }
    for(i = 0; i < vms.size(); i++){
        VMInfo_t vm = VM_GetInfo(vms[i]);
        MachineInfo_t m = Machine_GetInfo(vm.machine_id);
        if(m.memory_used > 0){
            for(TaskId_t task : vm.active_tasks){
                for(size_t j = vms.size() - 1; j > i; j--){ // we changed this
                     MachineInfo_t k = Machine_GetInfo(VM_GetInfo(vms[i]).machine_id);
                     TaskInfo_t t = GetTaskInfo(task);
                     if (k.memory_used + t.required_memory < k.memory_size) {
                        //migration!!! how exciting
                        VM_RemoveTask(vms[i], task);
                        VM_AddTask(vms[j], task, t.priority);
                     }
                }
                // for (int j = i + 1; j < vms.size(); j++) { }
            }
        }
        if(vm.active_tasks.empty()){
            vms.erase(vms.begin() + i);
            machines.erase(machines.begin() + i);
            active_machines--;
        }
    }
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
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
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
    static unsigned counts = 0;
    counts++;
    if(counts == 10) {
        migrating = true;
        VM_Migrate(1, 9);
    }
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
    // Called in response to an earlier request to change the state of a machine
}

