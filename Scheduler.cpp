//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
// greedy

// Greedily assigns tasks according to the algorithm outlined in lecture

#include "Scheduler.hpp"

#include <bits/stdc++.h>

#include <unordered_map>

// Data structure that holds the id and state of each machine along with the VMs
// running on it State is defined as the state the machine is transferring to
typedef struct {
  MachineId_t id;
  MachineState_t s;
  vector<VMId_t> vms;
} MachineVMs;

// This structure holds the machines classified by each CPU type
typedef struct {
  vector<MachineVMs> arm;
  vector<MachineVMs> power;
  vector<MachineVMs> riscv;
  vector<MachineVMs> x86;
} machine_cpus;

// This structure holds the pending tasks and memory used on transferring
// machines
typedef struct {
  vector<TaskId_t> tasks;
  size_t memory_used;
} tasks_and_memory;

// Unordered map to access the above data structure by MachineId_t in constant
// time
static unordered_map<MachineId_t, tasks_and_memory> pending;

// List of vectors containing tasks of each level of priorities
static vector<TaskId_t> high_gpu;
static vector<TaskId_t> high_pri;
static vector<TaskId_t> mid_pri;
static vector<TaskId_t> low_gpu;
static vector<TaskId_t> low_pri;

// global variable instantiation of machine_cpus
static machine_cpus mc;

// How often to migrate VMs
static int migrate_frequency = 500;
static int cycle = 0;

void Scheduler::Init() {

  // Outputs to aid debugging
  SimOutput("Scheduler::Init(): Total number of machines is " +
                to_string(Machine_GetTotal()),
            3);
  SimOutput("Scheduler::Init(): Initializing scheduler", 1);

  // Counter of each machine type
  size_t arm_cnt = 0;
  size_t x86_cnt = 0;
  size_t riscv_cnt = 0;
  size_t power_cnt = 0;
  size_t i;

  // Turn on fraction of machines based on the total number of machines of each
  // cpu_type
  for (i = 0; i < Machine_GetTotal(); i++) {
    MachineVMs machine;  // = {MachineId_t(i), {}};
    machine.id = MachineId_t(i);
    machine.s = S0;
    MachineState_t state_sleep = S3;
    size_t machine_cnt = 24;
    switch (Machine_GetCPUType(MachineId_t(i))) {
      case ARM:
        if (arm_cnt > machine_cnt) {
          machine.s = state_sleep;
          Machine_SetState(machine.id, state_sleep);
        }
        arm_cnt++;
        mc.arm.push_back(machine);
        break;
      case X86:
        if (x86_cnt > machine_cnt) {
          machine.s = state_sleep;
          Machine_SetState(machine.id, state_sleep);
        }
        x86_cnt++;
        mc.x86.push_back(machine);
        break;
      case RISCV:
        if (riscv_cnt > machine_cnt) {
          machine.s = state_sleep;
          Machine_SetState(machine.id, state_sleep);
        }
        riscv_cnt++;
        mc.riscv.push_back(machine);
        break;
      case POWER:
        if (power_cnt > machine_cnt) {
          machine.s = state_sleep;
          Machine_SetState(machine.id, state_sleep);
        }
        power_cnt++;
        mc.power.push_back(machine);
        break;
    }
  }
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
  for (i = 0; i < compat_machines->size(); i++) {
    if ((*compat_machines)[i].id == m) {
      break;
    }
  }
  (*compat_machines)[i].vms.push_back(vm_id);
}

/* This function is used to compare two MachineVMs based on their memory
   utilization 
   
   It returns true if the first machine has a higher memory
   utilization than the second one 
   
   This is used for sorting the machines in
   descending order of their memory utilization 
   
   Higher utilization machines will be prioritized for task allocation 
   
   This is useful for load balancing and ensuring efficient resource usage 
   
   Note: Usage in std::sort()
*/
bool dec_comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t a_info = Machine_GetInfo(a.id);
  MachineInfo_t b_info = Machine_GetInfo(b.id);
  float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
  float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
  return util_a > util_b;
}

/* This function finds a suitable machine for the given task_id based on the
   active flag 
   If active is true, it will only consider machines that are in S0.
   If active is false, it will consider machines that are not in S0 and can
   accommodate the task.
   It returns true if a suitable machine is found and assigns the task to it 
   Otherwise, it returns false 
   
   Note: This function also
   updates the pending tasks for machines when active is false
*/
bool Scheduler::FindMachine(TaskId_t task_id, bool active) {
  TaskInfo_t task = GetTaskInfo(task_id);
  vector<MachineVMs>* compat_machines = nullptr;

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

  if (!compat_machines) return false;

  // Sort the machines based on their memory utilization in descending order
  size_t i = 0;
  sort(compat_machines->begin(), compat_machines->end(), dec_comp);

  // Go through machines to find match
  for (i = 0; i < compat_machines->size(); i++) {
    MachineVMs& machine = (*compat_machines)[i];  // cleaner access
    MachineInfo_t m_info = Machine_GetInfo(machine.id);

    SimOutput("FindMachine " + to_string(m_info.machine_id) + " in state " +
                  to_string(m_info.s_state),
              4);
    SimOutput("FindMachine transitioning to " + to_string((size_t)machine.s),
              4);

    // Criteria to skip based on parameters
    if (active && (m_info.s_state != S0 || m_info.s_state != machine.s)) {
      continue;
    }
    if (!active && (m_info.s_state == S0 && m_info.s_state == machine.s)) {
      continue;
    }

    // Make sure theres enough memory available
    if (m_info.memory_used + task.required_memory < m_info.memory_size &&
        m_info.active_tasks < m_info.num_cpus) {
      bool allowed = active;
      allowed =
          allowed || (!active && pending.find(machine.id) == pending.end());
      allowed =
          allowed || (!active && pending.find(machine.id) != pending.end() &&
                      pending[machine.id].tasks.size() < m_info.num_cpus &&
                      pending[machine.id].memory_used + task.required_memory <
                          m_info.memory_size);

      if (!allowed) continue;

      if (!active) {
        // Add to pending if this machine is not S0
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

/* This function assigns tasks to machines based on their priority levels
   It processes tasks in the order of high_gpu, high_pri, mid_pri, low_gpu, and
   low_pri It tries to find a suitable machine for each task in the respective
   priority list Note: This function calls FindMachine() to find a suitable
   machine for each task
*/
void Scheduler::AssignTasks() {
  bool done = false;
  while (!done && !high_gpu.empty()) {
    if (!FindMachine(high_gpu[0], true)) {
      if (!FindMachine(high_gpu[0], false)) {
        done = true;
        break;
      }
    }
    high_gpu.erase(high_gpu.begin());
  }
  done = false;
  while (!done && !high_pri.empty()) {
    if (!FindMachine(high_pri[0], true)) {
      if (!FindMachine(high_pri[0], false)) {
        done = true;
        break;
      }
    }
    high_pri.erase(high_pri.begin());
  }
  done = false;
  while (!done && !mid_pri.empty()) {
    if (!FindMachine(mid_pri[0], true)) {
      if (!FindMachine(mid_pri[0], false)) {
        done = true;
        break;
      }
    }
    mid_pri.erase(mid_pri.begin());
  }
  done = false;
  while (!done && !low_gpu.empty()) {
    if (!FindMachine(low_gpu[0], true)) {
      if (!FindMachine(low_gpu[0], false)) {
        done = true;
        break;
      }
    }
    low_gpu.erase(low_gpu.begin());
  }
  done = false;
  while (!done && !low_pri.empty()) {
    if (!FindMachine(low_pri[0], true)) {
      if (!FindMachine(low_pri[0], false)) {
        done = true;
        break;
      }
    }
    low_pri.erase(low_pri.begin());
  }
}

/* Asynchronously called when a new task is added
   It checks the task's required SLA and GPU capability
   It adds the task to the appropriate priority list based on its SLA and GPU
   capability Finally, it calls AssignTasks() to assign tasks to machines
*/
void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
  // Get the task parameters
  TaskInfo_t task = GetTaskInfo(task_id);
  switch (task.required_sla) {
    case SLA0:
    case SLA1:
      if (task.gpu_capable) {
        high_gpu.push_back(task_id);
      } else {
        high_pri.push_back(task_id);
      }
      break;
    case SLA2:
      if (task.gpu_capable) {
        high_gpu.push_back(task_id);
      } else {
        mid_pri.push_back(task_id);
      }
      break;
    case SLA3:
      if (task.gpu_capable) {
        low_gpu.push_back(task_id);
      } else {
        low_pri.push_back(task_id);
      }
      break;
  }
  AssignTasks();
}

/* Asynchronously called every so often
   It checks if any tasks still need to be assigned, and assigns them
*/
void Scheduler::PeriodicCheck(Time_t now) {
  // This method should be called from SchedulerCheck()
  // SchedulerCheck is called periodically by the simulator to allow you to
  // monitor, make decisions, adjustments, etc. Unlike the other invocations of
  // the scheduler, this one doesn't report any specific event Recommendation:
  // Take advantage of this function to do some monitoring and adjustments as
  // necessary
  if ((!high_pri.empty() || !mid_pri.empty() || !low_pri.empty())) {
    SimOutput("Still some left", 4);
  }
  AssignTasks();
}

void Scheduler::Shutdown(Time_t time) {
  // Do your final reporting and bookkeeping here.
  // Report about the total energy consumed
  // Report about the SLA compliance
  // Shutdown everything to be tidy :-)
  for (MachineVMs mvm : mc.arm) {
    for (auto& vm : mvm.vms) {
      VM_Shutdown(vm);
    }
  }
  for (MachineVMs mvm : mc.x86) {
    for (auto& vm : mvm.vms) {
      VM_Shutdown(vm);
    }
  }
  for (MachineVMs mvm : mc.riscv) {
    for (auto& vm : mvm.vms) {
      VM_Shutdown(vm);
    }
  }
  for (MachineVMs mvm : mc.power) {
    for (auto& vm : mvm.vms) {
      VM_Shutdown(vm);
    }
  }
  SimOutput("SimulationComplete(): Finished!", 0);
  SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

// This function is used to compare two MachineVMs based on their memory
// utilization It returns true if the first machine has a lower memory
// utilization than the second one This is used for sorting the machines in
// ascending order of their memory utilization
bool comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t a_info = Machine_GetInfo(a.id);
  MachineInfo_t b_info = Machine_GetInfo(b.id);
  float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
  float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
  return util_a < util_b;
}

// This function is called when a task is completed

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
  // idea: migration is expensive. Only do migration every frequency tasks
  cycle++;
  if (cycle == migrate_frequency) {
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
      if (Machine_GetInfo(mvm.id).memory_used > 0 &&
          Machine_GetInfo(mvm.id).s_state == S0 && mvm.s == S0) {
        for (VMId_t vm : mvm.vms) {
          size_t task_sum = 0;
          for (TaskId_t t : VM_GetInfo(vm).active_tasks) {
            task_sum += GetTaskInfo(t).required_memory;
          }
          if (min_vm == dummy || task_sum > min_task_sum) {
            min_vm = vm;
            min_task_sum = task_sum;
            min_mac = mvm.id;
          }
        }
      }
    }
    if (min_vm == dummy) {
      return;
    }
    VMInfo_t min_vm_info = VM_GetInfo(min_vm);
    // we now have the smallest vm in the right half.
    // find if any vms in the left half can support this vm.
    bool found_vm;
    for (i = compat_machines->size() - 1; i >= compat_machines->size() / 2;
         i--) {
      MachineVMs& mvm = (*compat_machines)[i];
      MachineInfo_t mvm_info = Machine_GetInfo(mvm.id);
      if (mvm_info.memory_used > 0 && mvm_info.s_state == S0 && mvm.s == S0) {
        bool can_fit =
            (mvm_info.memory_used + min_task_sum < mvm_info.memory_size);
        can_fit = can_fit &&
                  (min_vm_info.active_tasks.size() + mvm_info.active_tasks <
                   mvm_info.num_cpus);
        if (mvm_info.memory_used + min_task_sum < mvm_info.memory_size) {
          // good vm
          found_vm = true;
          break;
        }
      }
    }
    if (found_vm && min_vm != dummy) {
      size_t i = 0;
      for (i = 0; i < compat_machines->size(); i++) {
        if ((*compat_machines)[i].id == min_mac) {
          break;
        }
      }
      if (i == compat_machines->size()) {
        SimOutput("w", 0);
      }
      size_t j = 0;
      bool found = false;
      for (j = 0; j < (*compat_machines)[i].vms.size(); j++) {
        if ((*compat_machines)[i].vms[j] == min_vm) {
          found = true;
          (*compat_machines)[i].vms.erase((*compat_machines)[i].vms.begin() +
                                          j);
          break;
        }
      }
      if (!found) {
        SimOutput("fdfsd", 0);
      }

      VM_Migrate(min_vm, (*compat_machines)[i].id);
    }
  }
  SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) +
                " is complete at " + to_string(now),
            4);
  SimOutput(
      "Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete",
      4);
}

// This function is called when a state change is complete
// It adds the pending tasks to the machine if updated state is S0
void Scheduler::ChangeComplete(Time_t now, MachineId_t machine_id) {
  MachineInfo_t m_info = Machine_GetInfo(machine_id);
  SimOutput("StateChangeComplete: Machine " + to_string(m_info.machine_id) +
                " in state " + to_string(m_info.s_state) + " ",
            4);
  SimOutput("StateChangeComplete: Machine has " +
                to_string(m_info.active_tasks) + " tasks ",
            4);
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
  for (i = 0; i < compat_machines->size(); i++) {
    if ((*compat_machines)[i].id == machine_id) {
      break;
    }
  }
  (*compat_machines)[i].s = m_info.s_state;
  if (m_info.s_state != S0 && pending.find(machine_id) != pending.end()) {
    (*compat_machines)[i].s = S0;
    Machine_SetState(machine_id, S0);
  } else if (m_info.s_state == S0) {
    if (i == compat_machines->size()) {
      (*compat_machines)[i].id = machine_id;
    }

    // exists
    vector<TaskId_t>& tasks = pending[machine_id].tasks;
    while (!tasks.empty()) {
      TaskInfo_t task = GetTaskInfo(tasks[0]);
      size_t j = 0;
      for (j = 0; j < (*compat_machines)[i].vms.size(); j++) {
        if (VM_GetInfo((*compat_machines)[i].vms[j]).vm_type ==
            task.required_vm) {
          // tasks_added++;
          // SimOutput("StateChangeComplete: " + to_string(tasks_added)
          // + " tasks added ", 0)
          VM_AddTask((*compat_machines)[i].vms[j], tasks[0], task.priority);
          break;
        }
      }
      if (j == (*compat_machines)[i].vms.size()) {
        (*compat_machines)[i].vms.push_back(
            VM_Create(task.required_vm, task.required_cpu));
        VM_Attach((*compat_machines)[i].vms[j], (*compat_machines)[i].id);
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
  SimOutput("HandleNewTask(): Received new task " + to_string(task_id) +
                " at time " + to_string(time),
            4);
  Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
  SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) +
                " completed at time " + to_string(time),
            4);
  Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
  // The simulator is alerting you that machine identified by machine_id is
  // overcommitted
  SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) +
                " was detected at time " + to_string(time),
            0);
}

void MigrationDone(Time_t time, VMId_t vm_id) {
  // The function is called on to alert you that migration is complete
  SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) +
                " was completed at time " + to_string(time),
            4);
  Scheduler.MigrationComplete(time, vm_id);
  // migrating = false;
}

void SchedulerCheck(Time_t time) {
  // This function is called periodically by the simulator, no specific event
  // SimOutput(to_string(time), 0);
  Scheduler.PeriodicCheck(time);
}

void SimulationComplete(Time_t time) {
  // This function is called before the simulation terminates Add whatever you
  // feel like.
  cout << "SLA violation report" << endl;
  cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
  cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
  cout << "SLA2: " << GetSLAReport(SLA2) << "%"
       << endl;  // SLA3 do not have SLA violation issues
  cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
  cout << "Simulation run finished in " << double(time) / 1000000 << " seconds"
       << endl;
  SimOutput(
      "SimulationComplete(): Simulation finished at time " + to_string(time),
      4);

  Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
  Scheduler.ChangeComplete(time, machine_id);
}
