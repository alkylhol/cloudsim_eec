//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.

//  PMapper

//  Fundamentally, similar to greedy. However, we define a fit-score
//  that considers how much performance we get per watt, and use that as a
//  heuristic to assigning tasks

#include "Scheduler.hpp"

#include <bits/stdc++.h>

#include <unordered_map>

// struct created to correlate machines to vms
typedef struct {
  MachineId_t id;
  MachineState_t s;
  vector<VMId_t> vms;
} MachineVMs;

// store machines by cpu type
typedef struct {
  vector<MachineVMs> arm;
  vector<MachineVMs> power;
  vector<MachineVMs> riscv;
  vector<MachineVMs> x86;
} machine_cpus;

// struct used to track tasks and memory of state-changing machines
typedef struct {
  vector<TaskId_t> tasks;
  size_t memory_used;
} tasks_and_memory;
// map used to track state changing machines
static unordered_map<MachineId_t, tasks_and_memory> pending;

// task queues
static vector<TaskId_t> high_gpu;
static vector<TaskId_t> high_pri;
static vector<TaskId_t> mid_pri;
static vector<TaskId_t> low_gpu;
static vector<TaskId_t> low_pri;

static int tasks_completed;
static bool migrating = false;  // debugging
static machine_cpus mc;
static int migrate_frequency = 500;  // migrate every how many tasks?
static int cycle = 0;

// keep track of machines in migration
static unordered_map<VMId_t, MachineId_t> in_migration;
static vector<MachineId_t> receiving;

/* PMapper's fit score. Factors in memory and cpu utilization,
   as well as MIPS per Watt. Also includes a penalty based on S state
*/
float fit_score(MachineInfo_t m_info) {
  float mem_util = (float)m_info.memory_used / (float)m_info.memory_size;
  float cpu_util = (float)m_info.active_tasks / (float)m_info.num_cpus;

  float power_penalty = (m_info.s_state == S0) ? 0.0f : 0.3f * m_info.s_state;

  float mips_per_s = (float)m_info.performance[0] / (float)m_info.s_states[0];
  return mips_per_s + mem_util + cpu_util - power_penalty;
}

/* Comparator for PMapper's fit score
 */
bool energy_comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t am = Machine_GetInfo(a.id);
  MachineInfo_t bm = Machine_GetInfo(b.id);
  return fit_score(am) > fit_score(bm);
}

/* Init: Sorts machines according to their CPU types. Then, we turn on a
   select amount of machines, which are previously sorted based on fit score.
*/
void Scheduler::Init() {
  SimOutput("Scheduler::Init(): Total number of machines is " +
                to_string(Machine_GetTotal()),
            3);
  SimOutput("Scheduler::Init(): Initializing scheduler", 1);
  size_t i;
  for (i = 0; i < Machine_GetTotal(); i++) {
    MachineVMs machine;  // = {MachineId_t(i), {}};
    machine.id = MachineId_t(i);
    machine.s = S0;
    switch (Machine_GetCPUType(MachineId_t(i))) {
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

  size_t machine_cnt = 16;
  MachineState_t state_sleep = S3;

  for (size_t i = 0; i < mc.arm.size(); i++) {
    if (i > machine_cnt) {
      mc.arm[i].s = state_sleep;
      Machine_SetState(mc.arm[i].id, state_sleep);
    }
  }
  for (size_t i = 0; i < mc.x86.size(); i++) {
    if (i > machine_cnt) {
      mc.x86[i].s = state_sleep;
      Machine_SetState(mc.x86[i].id, state_sleep);
    }
  }
  for (size_t i = 0; i < mc.power.size(); i++) {
    if (i > machine_cnt) {
      mc.power[i].s = state_sleep;
      Machine_SetState(mc.power[i].id, state_sleep);
    }
  }
  for (size_t i = 0; i < mc.riscv.size(); i++) {
    if (i > machine_cnt) {
      mc.riscv[i].s = state_sleep;
      Machine_SetState(mc.riscv[i].id, state_sleep);
    }
  }
}

/* Update MachineVMs.vms and the migration data structures. */
void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
  // Update your data structure. The VM now can receive new tasks
  MachineId_t m = VM_GetInfo(vm_id).machine_id;
  MachineInfo_t m_info = Machine_GetInfo(m);
  vector<MachineVMs>* compat_machines;
  in_migration.erase(vm_id);
  auto it = find(receiving.begin(), receiving.end(), m);
  if (it != receiving.end()) {
    receiving.erase(it);  // Erase by iterator
  }

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

/* Comparator method to sort list of MachineVMs by decreasing utilization */
bool dec_comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t a_info = Machine_GetInfo(a.id);
  MachineInfo_t b_info = Machine_GetInfo(b.id);
  float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
  float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
  return util_a > util_b;
}

/* Finds a machine that can support this task. Receives a task id
   and a boolean defining whether we should search only active macines
   When active is false, we put tasks on pending.

   Returns a boolean that indicates whether we successfully found a
   machine.

   We sort by energy_comp in order to find the best machine according to
   PMapper
   */
bool Scheduler::FindMachine(TaskId_t task_id, bool active) {
  TaskInfo_t task = GetTaskInfo(task_id);
  vector<MachineVMs>* compat_machines = nullptr;

  // first, find machines with compatible CPU
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

  size_t i = 0;
  sort(compat_machines->begin(), compat_machines->end(), energy_comp);

  for (i = 0; i < compat_machines->size(); i++) {
    MachineVMs& machine = (*compat_machines)[i];  // cleaner access
    MachineInfo_t m_info = Machine_GetInfo(machine.id);
    SimOutput("FindMachine " + to_string(m_info.machine_id) + " in state " +
                  to_string(m_info.s_state),
              4);
    SimOutput("FindMachine transitioning to " + to_string((size_t)machine.s),
              4);

    if (active && (m_info.s_state != S0 || m_info.s_state != machine.s)) {
      continue;
    }
    if (!active && (m_info.s_state == S0 && m_info.s_state == machine.s)) {
      continue;
    }

    if (m_info.memory_used + task.required_memory < m_info.memory_size &&
        m_info.active_tasks < m_info.num_cpus) {
      // for sleeping machines, check pending list to see if task can fit
      bool allowed = active;
      allowed =
          allowed || (!active && pending.find(machine.id) == pending.end());
      allowed =
          allowed || (!active && pending.find(machine.id) != pending.end() &&
                      pending[machine.id].tasks.size() < m_info.num_cpus &&
                      pending[machine.id].memory_used + task.required_memory <
                          m_info.memory_size);

      if (!allowed) continue;

      // put on pending
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

/* Assign tasks to machines. GPU tasks prioritized
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

/* Process a new task. Put on different queues depending on gpu-capability and
   SLA
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

/* Checks whether a method is in migration */
bool InMigration(MachineVMs mvm) {
  bool found = false;
  for (const auto& pair : in_migration) {
    if (pair.second == mvm.id) {
      found = true;
      break;
    }
  }
  for (MachineId_t m : receiving) {
    if (m == mvm.id) {
      found = true;
      break;
    }
  }
  return found;
}

/* Periodically assign tasks */
void Scheduler::PeriodicCheck(Time_t now) {
  if ((!high_pri.empty() || !mid_pri.empty() || !low_pri.empty())) {
    SimOutput("Still some left", 4);
  }
  AssignTasks();
}

void Scheduler::Shutdown(Time_t time) {
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

/* Comparator method for sorting machines in ascending utilization order */
bool comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t a_info = Machine_GetInfo(a.id);
  MachineInfo_t b_info = Machine_GetInfo(b.id);
  float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
  float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
  return util_a < util_b;
}

/* Upon task completion, migrate a lower util task to a higher util machine
 */
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
      in_migration[min_vm] = min_mac;
      receiving.push_back((*compat_machines)[i].id);
      VM_Migrate(min_vm, (*compat_machines)[i].id);
    }
  }
  SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) +
                " is complete at " + to_string(now),
            4);
  SimOutput(
      "Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete",
      4);
  tasks_completed++;
  SimOutput("Scheduler::TaskComplete(): " + to_string(tasks_completed) +
                " tasks completed ",
            4);
}

/* Processes a completed state change. If machine is now in S0, then
   put pending tasks on machine.
*/
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
  migrating = false;
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
  // SimOutput(to_string(time), 0);
  Scheduler.ChangeComplete(time, machine_id);
}
