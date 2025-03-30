// //
// //  Scheduler.cpp
// //  CloudSim
// //
// //  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
// //
// // e-eco

// #include "Scheduler.hpp"
// #include "cmath"
// #include <unordered_map>

// static bool migrating = false;

// typedef struct {
//     MachineId_t id;
//     MachineState_t s;
//     vector<VMId_t> vms;
// } MachineVMs;

// typedef struct {
//     vector<MachineVMs> arm;
//     vector<MachineVMs> power;
//     vector<MachineVMs> riscv;
//     vector<MachineVMs> x86;
// } machine_cpus;

// typedef struct {
//     vector<TaskId_t> tasks;
//     size_t memory_used;
// } tasks_and_memory;

// static unordered_map<MachineId_t, tasks_and_memory> pending;

// static machine_cpus mc;

// void Scheduler::Init() {
//     // Find the parameters of the clusters
//     // Get the total number of machines
//     // For each machine:
//     //      Get the type of the machine
//     //      Get the memory of the machine
//     //      Get the number of CPUs
//     //      Get if there is a GPU or not
//     // 
//     SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
//     SimOutput("Scheduler::Init(): Initializing scheduler", 1);
//     for (size_t i = 0; i < Machine_GetTotal(); i++) {
//         MachineVMs mvm;
//         mvm.id = MachineId_t(i);
//         mvm.vms.clear();
//         switch (Machine_GetCPUType(MachineId_t(i))) {
//             case ARM:
//                 mc.arm.push_back(mvm); 
//                 break;
//             case X86:
//                 mc.x86.push_back(mvm);
//                 break;
//             case RISCV:
//                 mc.riscv.push_back(mvm);
//                 break;
//             case POWER:
//                 mc.power.push_back(mvm);
//                 break;
//         }
//     }
//     float fraction = 1.0f;
//     size_t arm_max = ceil(mc.arm.size() * fraction);
//     size_t x86_max = ceil(mc.arm.size() * fraction);
//     size_t riscv_max = ceil(mc.arm.size() * fraction);
//     size_t power_max = ceil(mc.arm.size() * fraction);
    
//     // for (size_t i = 0; i < mc.arm.size(); i++) {
//     //     if (i < arm_max) {
//     //         Machine_SetState(mc.arm[i].id, S3);
//     //         mc.arm[i].s = S3;
//     //     } else {
//     //         Machine_SetState(mc.arm[i].id, S5);
//     //         mc.arm[i].s = S5;
//     //     }
//     // } 
//     // for (size_t i = 0; i < mc.x86.size(); i++) {
//     //     if (i < x86_max) {
//     //         Machine_SetState(mc.x86[i].id, S3);
//     //         mc.x86[i].s = S3;
//     //     } else {
//     //         Machine_SetState(mc.x86[i].id, S5);
//     //         mc.x86[i].s = S5;
//     //     }
//     // } 
//     // for (size_t i = 0; i < mc.riscv.size(); i++) {
//     //     if (i < riscv_max) {
//     //         Machine_SetState(mc.riscv[i].id, S3);
//     //         mc.riscv[i].s = S3;
//     //     } else {
//     //         Machine_SetState(mc.riscv[i].id, S5);
//     //         mc.riscv[i].s = S5;
//     //     }
//     // }
//     // for (size_t i = 0; i < mc.power.size(); i++) {
//     //     if (i < power_max) {
//     //         Machine_SetState(mc.power[i].id, S3);
//     //         mc.power[i].s = S3;
//     //     } else {
//     //         Machine_SetState(mc.power[i].id, S5);
//     //         mc.power[i].s = S5;
//     //     }
//     // }
//     SimOutput("Finished Scheduler::Init()", 2);
// }

// void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
//     // Update your data structure. The VM now can receive new tasks
// }

// void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
//     SimOutput("Inside New Task", 2);
//     TaskInfo_t task = GetTaskInfo(task_id);
//     Priority_t priority = task.required_sla <= SLA1 ? HIGH_PRIORITY : 
//                             task.required_sla == SLA2 ? MID_PRIORITY : LOW_PRIORITY;
//     vector<MachineVMs>* compat_machines = &mc.arm;
//     switch (task.required_cpu) {
//         case ARM:
//             compat_machines = &mc.arm;
//             break;
//         case X86:
//             compat_machines = &mc.x86;
//             break;
//         case RISCV:
//             compat_machines = &mc.riscv;
//             break;
//         case POWER:
//             compat_machines = &mc.power;
//             break;
//     }
    
//     for (size_t i = 0; i < compat_machines->size(); i++) {
//         string vm_list_str = "VMs: ";
//         for (const auto& vm_id : (*compat_machines)[i].vms) {
//             vm_list_str += to_string(vm_id) + " ";
//         }
//         //SimOutput(vm_list_str, 2);
//         MachineInfo_t machine_info = Machine_GetInfo((*compat_machines)[i].id);
//         //pre-condition, machine has enough memory
//         if (machine_info.memory_used + task.required_memory > machine_info.memory_size) {
//             continue;
//         }
//         //pre-condition, check if there are any pending tasks on this machine
//         if (pending.find((*compat_machines)[i].id) != pending.end()) {
//             //this has some pending tasks, also make sure they dont go over max memory
//             if (pending[(*compat_machines)[i].id].memory_used + 
//                 task.required_memory + machine_info.memory_used > 
//                 machine_info.memory_size) {
//                 continue;
//             }
//             if (machine_info.active_tasks + pending[(*compat_machines)[i].id].tasks.size() > machine_info.num_cpus){
//                 continue;
//             }
//         }
//         //pre-condition, check if more tasks than cores
//         if (machine_info.active_tasks >= machine_info.num_cpus) {
//             continue;
//         }
//         if ((*compat_machines)[i].s == S0 && machine_info.s_state == S0) {
//             //machine is fully awake, and is planning on staying awake
//             //can directly place task on
//             SimOutput("found valid machine that is awake", 0);
//             vector<VMId_t> vm_list = (*compat_machines)[i].vms;
//             for (size_t j = 0; j < vm_list.size(); j++) {
//                 VMInfo_t curr_vm = VM_GetInfo(vm_list[j]);
//                 if (curr_vm.vm_type == task.required_vm) {
//                     //found a vm, put it on and end the method
//                     VM_AddTask(vm_list[j], task_id, priority);
//                     return;
//                 }
//             }
//             // if we get here, there were no compatible vms
//             // we have to make one
//             VMId_t new_vm = VM_Create(task.required_vm, task.required_cpu);
//             (*compat_machines)[i].vms.push_back(new_vm);
//             SimOutput("Added VM " + to_string(new_vm) + " to machine " + to_string((*compat_machines)[i].id), 2);

//             VM_Attach(new_vm, (*compat_machines)[i].id);
            
//             // Now add the task
//             VM_AddTask(new_vm, task_id, priority);
//             return;
//         } else if ((*compat_machines)[i].s == S0 && machine_info.s_state != S0) {
//             SimOutput("found valid machine transitioning to awake", 2);
//             //machine is transitioning to the awake state
//             //need to add task to queue
//             if (pending.find((*compat_machines)[i].id) == pending.end()) {
//                 //entry does not already exist (no pending tasks)
//                 tasks_and_memory tandm;
//                 tandm.tasks.push_back(task_id);
//                 tandm.memory_used = task.required_memory;
//                 pending[(*compat_machines)[i].id] = tandm;
//             } else {
//                 //entry already exists (pending tasks)
//                 pending[(*compat_machines)[i].id].tasks.push_back(task_id);
//                 pending[(*compat_machines)[i].id].memory_used += task.required_memory;
//             }
//             return;
//         } else {
//             //machine is in intermediate or asleep state, skip
//         }
//     }
//     // if we are here, that means we need to use one of the standby machines,
//     // not enough machines were in S0
//     for (size_t i = 0; i < compat_machines->size(); i++) {
//         //we want machines in S3 or transitioning to S3
//         MachineInfo_t machine_info = Machine_GetInfo((*compat_machines)[i].id);
//         //pre-condition, machine has enough memory
//         if (machine_info.memory_used + task.required_memory > machine_info.memory_size) {
//             continue;
//         }
//         if (machine_info.active_tasks >= machine_info.num_cpus) {
//             continue;
//         }
//         if (pending.find((*compat_machines)[i].id) != pending.end()) {
//             //this has some pending tasks, also make sure they dont go over max memory
//             if (pending[(*compat_machines)[i].id].memory_used + 
//                 task.required_memory + machine_info.memory_used > 
//                 machine_info.memory_size) {
//                 continue;
//             }
//             if (machine_info.active_tasks + pending[(*compat_machines)[i].id].tasks.size() > machine_info.num_cpus){
//                 continue;
//             }
//         }
//         if ((*compat_machines)[i].s == S3 && machine_info.s_state == S3 ) {
//             SimOutput("found valid machine in standby", 2);
//             //in s3, should be in s3
//             //wake it up and assign the task
//             if (pending.find((*compat_machines)[i].id) == pending.end()) {
//                 //entry does not already exist (no pending tasks)
//                 tasks_and_memory tandm;
//                 tandm.tasks.push_back(task_id);
//                 tandm.memory_used = task.required_memory;
//                 pending[(*compat_machines)[i].id] = tandm;
//                 SimOutput("Created new pending entry for machine " + to_string((*compat_machines)[i].id) + 
//                           " with task " + to_string(task_id), 2);
//             } else {
//                 //entry already exists (pending tasks)
//                 pending[(*compat_machines)[i].id].tasks.push_back(task_id);
//                 pending[(*compat_machines)[i].id].memory_used += task.required_memory;
//                 SimOutput("Created new pending entry for machine " + to_string((*compat_machines)[i].id) + 
//                 " with task " + to_string(task_id), 2);
//             }
//             (*compat_machines)[i].s = S0;
//             Machine_SetState((*compat_machines)[i].id, S0);
//             return;
//         } else if ((*compat_machines)[i].s == S3 && machine_info.s_state != S3) {
//             SimOutput("found valid machine transitioning to standby", 2);
//             //transitioning to S3
//             if (pending.find((*compat_machines)[i].id) == pending.end()) {
//                 //entry does not already exist (no pending tasks)
//                 tasks_and_memory tandm;
//                 tandm.tasks.push_back(task_id);
//                 tandm.memory_used = task.required_memory;
//                 pending[(*compat_machines)[i].id] = tandm;
//                 SimOutput("Created new pending entry for machine " + to_string((*compat_machines)[i].id) + 
//                           " with task " + to_string(task_id), 2);
//             } else {
//                 //entry already exists (pending tasks)
//                 pending[(*compat_machines)[i].id].tasks.push_back(task_id);
//                 pending[(*compat_machines)[i].id].memory_used += task.required_memory;
//                 SimOutput("Created new pending entry for machine " + to_string((*compat_machines)[i].id) + 
//                           " with task " + to_string(task_id), 2);
//             }
//             return;
//         }
//     }
// }

// void Scheduler::PeriodicCheck(Time_t now) {
//     // This method should be called from SchedulerCheck()
//     // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
//     // Unlike the other invocations of the scheduler, this one doesn't report any specific event
//     // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    
// }

// void Scheduler::Shutdown(Time_t time) {
//     // Do your final reporting and bookkeeping here.
//     // Report about the total energy consumed
//     // Report about the SLA compliance
//     //Shutdown everything to be tidy :-)
//     for(auto & vm: vms) {
//         VM_Shutdown(vm);
//     }
//     SimOutput("SimulationComplete(): Finished!", 4);
//     SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
// }

// void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
//     // Do any bookkeeping necessary for the data structures
//     // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
//     // This is an opportunity to make any adjustments to optimize performance/energy
//     SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 0);
// }

// void Scheduler::CompletedStateChange(MachineId_t machine_id) {
//     MachineInfo_t machine_info = Machine_GetInfo(machine_id);
//     if (machine_info.s_state == S0 && pending.find(machine_id) != pending.end()) {
//         //just woke up, and has pending tasks
//         for (auto it = pending[machine_id].tasks.begin(); it != pending[machine_id].tasks.end();) {
//             TaskInfo_t task = GetTaskInfo(*it); // Get the task info for the task id
//             Priority_t priority = task.required_sla <= SLA1 ? HIGH_PRIORITY : 
//                             task.required_sla == SLA2 ? MID_PRIORITY : LOW_PRIORITY;
//             //find the MachineVM data struct for this id
//             vector<MachineVMs>* compat_machines = nullptr;
//             MachineVMs* curr_machine = nullptr; // Default to the first one in case we don't find it
//             switch(task.required_cpu) {
//                 case ARM: 
//                     compat_machines = &mc.arm;
//                     break;
//                 case X86: 
//                     compat_machines = &mc.x86;
//                     break;
//                 case RISCV: 
//                     compat_machines = &mc.riscv;
//                     break;
//                 case POWER: 
//                     compat_machines = &mc.power;
//                     break;
//             }
//             for (size_t i = 0; i < compat_machines->size(); i++) {
//                 if ((*compat_machines)[i].id == machine_id) {
//                     curr_machine = &(*compat_machines)[i];
//                     break;
//                 }
//             }
//             //add task to vm/machine
//             vector<VMId_t>& vm_list = curr_machine->vms;
//             size_t j;
//             SimOutput(to_string(vm_list.size()) + " VMs on machine " + to_string(machine_id) + " for task " + to_string(task.task_id), 2);
//             for (j = 0; j < vm_list.size(); j++) {
//                 VMInfo_t curr_vm = VM_GetInfo(vm_list[j]);
//                 if (curr_vm.vm_type == task.required_vm) {
//                     //found a vm, put it on and end the method
//                     VM_AddTask(vm_list[j], task.task_id, priority);
                    
//                     break;
//                 }
//             }
//             if (j == vm_list.size()) {
//                 // if we get here, there were no compatible vms
//                 // we have to make one
//                 VMId_t new_vm = VM_Create(task.required_vm, task.required_cpu);
//                 curr_machine->vms.push_back(new_vm);
//                 SimOutput("Added VM " + to_string(new_vm) + " to machine " + to_string((*curr_machine).id), 2);
//                 VM_Attach(new_vm, (*curr_machine).id);
//                 // Now add the task
//                 SimOutput("Added task " + to_string(task.task_id) + " to VM " + to_string(new_vm) + 
//                           " on machine " + to_string(machine_id), 2);
//                 VM_AddTask(new_vm, task.task_id, priority);
//             }
//             //SimOutput("Removing task " + to_string(*it) + " from pending on machine " + to_string(machine_id), 2);
//             it = pending[machine_id].tasks.erase(it);
//             if (pending[machine_id].tasks.empty()) {
//                 pending.erase(machine_id);
//                 break; // Exit the loop since the entry no longer exists
//             }
//         }
//     }
//     if (machine_info.s_state == S3 && pending.find(machine_id) != pending.end()) {
//         Machine_SetState(machine_id, S0);
//         MachineVMs* curr_machine = nullptr; // Default to the first one in case we don't find it
//         bool found = false;
//         if (!found) {
//             for (size_t i = 0; i < mc.arm.size(); i++) {
//                 if (mc.arm[i].id == machine_id) {
//                     curr_machine = &mc.arm[i];
//                     found = true;
//                     break;
//                 }
//             }
//         }
//         if (!found) {
//             for (size_t i = 0; i < mc.x86.size(); i++) {
//                 if (mc.x86[i].id == machine_id) {
//                     curr_machine = &mc.x86[i];
//                     found = true;
//                     break;
//                 }
//             }
//         } 
//         if (!found) {
//             for (size_t i = 0; i < mc.riscv.size(); i++) {
//                 if (mc.riscv[i].id == machine_id) {
//                     curr_machine = &mc.riscv[i];
//                     found = true;
//                     break;
//                 }
//             }
//         }
//         if (!found) {
//             for (size_t i = 0; i < mc.power.size(); i++) {
//                 if (mc.power[i].id == machine_id) {
//                     curr_machine = &mc.power[i];
//                     found = true;
//                     break;
//                 }
//             }
//         }
//         curr_machine->s = S0;
//     }
// }

// // Public interface below

// static Scheduler Scheduler;

// void InitScheduler() {
//     SimOutput("InitScheduler(): Initializing scheduler", 4);
//     Scheduler.Init();
// }

// void HandleNewTask(Time_t time, TaskId_t task_id) {
//     SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
//     Scheduler.NewTask(time, task_id);
// }

// void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
//     SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
//     Scheduler.TaskComplete(time, task_id);
// }

// void MemoryWarning(Time_t time, MachineId_t machine_id) {
//     // The simulator is alerting you that machine identified by machine_id is overcommitted
//     SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
// }

// void MigrationDone(Time_t time, VMId_t vm_id) {
//     // The function is called on to alert you that migration is complete
//     SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
//     Scheduler.MigrationComplete(time, vm_id);
//     migrating = false;
// }

// void SchedulerCheck(Time_t time) {
//     // This function is called periodically by the simulator, no specific event
//     SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
//     Scheduler.PeriodicCheck(time);
// }

// void SimulationComplete(Time_t time) {
//     // This function is called before the simulation terminates Add whatever you feel like.
//     cout << "SLA violation report" << endl;
//     cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
//     cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
//     cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
//     cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
//     cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
//     SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
//     Scheduler.Shutdown(time);
// }

// void SLAWarning(Time_t time, TaskId_t task_id) {
//     //SimOutput("SLAWarning(): SLA violation for task " + to_string(task_id) + 
//     //          " detected at time " + to_string(time), 1);
// }

// void StateChangeComplete(Time_t time, MachineId_t machine_id) {
//     // Called in response to an earlier request to change the state of a machine
//     SimOutput("here", 2);
//     Scheduler.CompletedStateChange(machine_id);
// }
