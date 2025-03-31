//  Scheduler.cpp
//  CloudSim

//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//  E-Eco

//  This scheduler implements the E-Eco algorithm as defined in lecture
//  We create 'bins' of tasks, ensuring that the ratio of machines in each bin
//  is maintained.

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

static int tasks_completed;
static bool migrating = false;

// global variable instantiation of machine_cpus
static machine_cpus mc;
static int migrate_frequency = 500;
static int cycle = 0;

// Fraction of machines we want to start in S0 and S3
static float s0_frac = 0.90f;
static float s3_frac = 0.05f;

// map of migration
static unordered_map<VMId_t, MachineId_t> in_migration;
static vector<MachineId_t> receiving;

// Function to compare two machines based on their memory utilization
void Scheduler::Init() {
  SimOutput("Scheduler::Init(): Total number of machines is " +
                to_string(Machine_GetTotal()),
            3);
  SimOutput("Scheduler::Init(): Initializing scheduler", 1);
  for (size_t i = 0; i < Machine_GetTotal(); i++) {
    MachineVMs mvm;
    mvm.id = MachineId_t(i);
    mvm.vms.clear();
    switch (Machine_GetCPUType(MachineId_t(i))) {
      case ARM:
        mc.arm.push_back(mvm);
        break;
      case X86:
        mc.x86.push_back(mvm);
        break;
      case RISCV:
        mc.riscv.push_back(mvm);
        break;
      case POWER:
        mc.power.push_back(mvm);
        break;
    }
  }

  size_t arm_on =
      ceil(mc.arm.size() * s0_frac);  // Number of ARM machines to turn on
  size_t x86_on =
      ceil(mc.x86.size() * s0_frac);  // Number of X86 machines to turn on
  size_t riscv_on =
      ceil(mc.riscv.size() * s0_frac);  // Number of RISCV machines to turn on
  size_t power_on =
      ceil(mc.power.size() * s0_frac);  // Number of POWER machines to turn on

  size_t arm_standby = ceil(mc.arm.size() * s3_frac) +
                       arm_on;  // Remaining ARM machines to put in standby
  size_t x86_standby = ceil(mc.x86.size() * s3_frac) +
                       x86_on;  // Remaining X86 machines to put in standby
  size_t riscv_standby =
      ceil(mc.riscv.size() * s3_frac) +
      riscv_on;  // Remaining RISCV machines to put in standby
  size_t power_standby =
      ceil(mc.power.size() * s3_frac) +
      power_on;  // Remaining POWER machines to put in standby

  for (size_t i = 0; i < mc.arm.size(); i++) {
    if (i < arm_on) {
      Machine_SetState(mc.arm[i].id, S0);
      mc.arm[i].s = S0;  // Fully awake
    } else if (i < arm_standby) {
      Machine_SetState(mc.arm[i].id, S3);
      mc.arm[i].s = S3;  // Standby
    } else {
      Machine_SetState(mc.arm[i].id, S5);  // Set to S5 (off)
      mc.arm[i].s = S5;                    // Off, not in use
    }
  }
  for (size_t i = 0; i < mc.x86.size(); i++) {
    if (i < x86_on) {
      Machine_SetState(mc.x86[i].id, S0);
      mc.x86[i].s = S0;  // Fully awake
    } else if (i < x86_standby) {
      Machine_SetState(mc.x86[i].id, S3);
      mc.x86[i].s = S3;  // Standby
    } else {
      Machine_SetState(mc.x86[i].id, S5);  // Set to S5 (off)
      mc.x86[i].s = S5;                    // Off, not in use
    }
  }
  for (size_t i = 0; i < mc.riscv.size(); i++) {
    if (i < riscv_on) {
      Machine_SetState(mc.riscv[i].id, S0);
      mc.riscv[i].s = S0;  // Fully awake
    } else if (i < riscv_standby) {
      Machine_SetState(mc.riscv[i].id, S3);
      mc.riscv[i].s = S3;  // Standby
    } else {
      Machine_SetState(mc.riscv[i].id, S5);  // Set to S5 (off)
      mc.riscv[i].s = S5;                    // Off, not in use
    }
  }
  for (size_t i = 0; i < mc.power.size(); i++) {
    if (i < power_on) {
      Machine_SetState(mc.power[i].id, S0);
      mc.power[i].s = S0;  // Fully awake
    } else if (i < power_standby) {
      Machine_SetState(mc.power[i].id, S3);
      mc.power[i].s = S3;  // Standby
    } else {
      Machine_SetState(mc.power[i].id, S5);  // Set to S5 (off)
      mc.power[i].s = S5;                    // Off, not in use
    }
  }

  SimOutput("Finished Scheduler::Init()", 2);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
  // Update your data structure. The VM now can receive new tasks
  MachineId_t m = VM_GetInfo(vm_id).machine_id;
  MachineInfo_t m_info = Machine_GetInfo(m);
  in_migration.erase(vm_id);
  auto it = std::find(receiving.begin(), receiving.end(), m);
  if (it != receiving.end()) {
    receiving.erase(it);  // Erase by iterator
  }
  vector<MachineVMs>* compat_machines;
  // Get correct cpu_type list
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

/* This function is used to compare two machines based on their memory
   utilization. It returns true if the first machine has a higher memory
   utilization than the second machine. Note: descending order
*/
bool dec_comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t a_info = Machine_GetInfo(a.id);
  MachineInfo_t b_info = Machine_GetInfo(b.id);
  float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
  float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
  return util_a > util_b;
}

/* This function is used to find a machine that can run the task with the given
   task_id. It checks if the machine is in the correct state (active, deep
   sleep, etc.) and if it has enough resources. If a suitable machine is found,
   it assigns the task to that machine or adds it to pending and returns true.
   If no suitable machine is found, it returns false.
*/
bool Scheduler::FindMachine(TaskId_t task_id, bool active, bool deep_sleep) {
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

  size_t i = 0;
  sort(compat_machines->begin(), compat_machines->end(), dec_comp);
  // goes through list of machines
  for (i = 0; i < compat_machines->size(); i++) {
    MachineVMs& machine = (*compat_machines)[i];  // cleaner access
    MachineInfo_t m_info = Machine_GetInfo(machine.id);

    SimOutput("FindMachine " + to_string(m_info.machine_id) + " in state " +
                  to_string(m_info.s_state),
              4);
    SimOutput("FindMachine transitioning to " + to_string((size_t)machine.s),
              4);
    // active: looking for S0 machine
    // deep_sleep: looking for S5 machine
    // !active && !deep_sleep: looking for S3 machine
    if (active && !deep_sleep &&
        (m_info.s_state != S0 || m_info.s_state != machine.s)) {
      continue;
    }
    if (!active && !deep_sleep && (m_info.s_state != S3)) {
      continue;
    }
    if (deep_sleep && m_info.s_state != S5) {
      continue;
    }
    if (task.gpu_capable && !m_info.gpus) {
      continue;
    }
    // Check if the machine has enough memory and CPU resources
    if (m_info.memory_used + task.required_memory < m_info.memory_size &&
        m_info.active_tasks < m_info.num_cpus) {
      // if looking for asleep, make sure not too many pending tasks
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
        // add to pending
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
        // add to actual machine
        size_t j = 0;
        for (j = 0; j < machine.vms.size(); j++) {
          if (VM_GetInfo(machine.vms[j]).vm_type == task.required_vm) {
            SimOutput("Adding to ID " + to_string(machine.id), 4);
            VM_AddTask(machine.vms[j], task_id, task.priority);
            return true;
          }
        }
        // if we are here, we did not find a vm of the right type
        if (j == machine.vms.size()) {
          // create vm and attach
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

// This function assigns tasks to machines based on their priority and GPU
// capability
void Scheduler::AssignTasks() {
  bool done = false;
  while (!done && !high_gpu.empty()) {
    if (!FindMachine(high_gpu[0], true, false)) {
      if (!FindMachine(high_gpu[0], false, false)) {
        if (!FindMachine(high_gpu[0], false, true)) {
          done = true;
          break;
        }
      }
    }
    high_gpu.erase(high_gpu.begin());
  }

  done = false;
  while (!done && !high_pri.empty()) {
    if (!FindMachine(high_pri[0], true, false)) {
      if (!FindMachine(high_pri[0], false, false)) {
        if (!FindMachine(high_pri[0], false, true)) {
          done = true;
          break;
        }
      }
    }
    high_pri.erase(high_pri.begin());
  }

  done = false;
  while (!done && !mid_pri.empty()) {
    if (!FindMachine(mid_pri[0], true, false)) {
      if (!FindMachine(mid_pri[0], false, false)) {
        if (!FindMachine(mid_pri[0], false, true)) {
          done = true;
          break;
        }
      }
    }
    mid_pri.erase(mid_pri.begin());
  }

  done = false;
  while (!done && !low_gpu.empty()) {
    if (!FindMachine(low_gpu[0], true, false)) {
      if (!FindMachine(low_gpu[0], false, false)) {
        if (!FindMachine(low_gpu[0], false, true)) {
          done = true;
          break;
        }
      }
    }
    low_gpu.erase(low_gpu.begin());
  }

  done = false;
  while (!done && !low_pri.empty()) {
    if (!FindMachine(low_pri[0], true, false)) {
      if (!FindMachine(low_pri[0], false, false)) {
        if (!FindMachine(low_pri[0], false, true)) {
          done = true;
          break;
        }
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

bool InMigration(MachineVMs mvm) {
  bool found = false;
  for (const auto& pair : in_migration) {
    if (pair.second == mvm.id) {
      found = true;
      break;  // remove this if you want to find all matches
    }
  }
  for (MachineId_t m : receiving) {
    if (m == mvm.id) {
      found = true;
      break;  // remove this if you want to find all matches
    }
  }
  return found;
}

// This function turns off machines in S0 if the number of machines in S0
// exceeds a certain threshold
void TurnOffMachines(vector<MachineVMs>& cpu_type) {
  size_t amt = 0;
  // Count the number of machines in S0
  for (size_t i = 0; i < cpu_type.size(); i++) {
    MachineInfo_t m_info = Machine_GetInfo(cpu_type[i].id);
    if (cpu_type[i].s == S0) {
      amt++;
    }
  }
  size_t s0_threshold = ceil(cpu_type.size() * s0_frac);
  for (MachineVMs& mvm : cpu_type) {
    if (amt > s0_threshold) {
      bool found = InMigration(mvm);
      if (!found && mvm.s == S0) {
        MachineInfo_t m_info = Machine_GetInfo(mvm.id);
        if (m_info.active_tasks == 0 && m_info.s_state == S0) {
          while (!mvm.vms.empty()) {
            mvm.vms.erase(mvm.vms.begin());
          }
          mvm.s = S3;
          Machine_SetState(mvm.id, S3);
        }
      }
    }
    amt--;
  }
}
// This function adjusts the percentage of machines in S3 and S5 states
void AdjustPercentage(vector<MachineVMs>& cpu_type) {
  size_t amt_in_s0 = 0;
  size_t amt_in_s3 = 0;
  size_t amt_in_s5 = 0;
  // Count the number of machines in S0, S3, and S5
  for (size_t i = 0; i < cpu_type.size(); i++) {
    MachineInfo_t m_info = Machine_GetInfo(cpu_type[i].id);
    if (cpu_type[i].s == S0) {
      amt_in_s0++;
    } else if (cpu_type[i].s == S3) {
      amt_in_s3++;
    } else if (cpu_type[i].s == S5) {
      amt_in_s5++;
    }
  }
  // check threshold of S3
  size_t s3_threshold = ceil(cpu_type.size() * s3_frac);
  if (amt_in_s3 > s3_threshold) {
    // too many? move down
    SimOutput("Need to move machines from S3 to S5, current S3 count: " +
                  to_string(amt_in_s3) +
                  " threshold: " + to_string(s3_threshold) +
                  " current S5 count " + to_string(amt_in_s5),
              4);
    for (size_t i = 0; i < cpu_type.size(); i++) {
      if (cpu_type[i].s == S3 &&
          Machine_GetInfo(cpu_type[i].id).s_state == S3 &&
          !InMigration(cpu_type[i])) {
        cpu_type[i].s = S5;  // Change to S5
        Machine_SetState(cpu_type[i].id,
                         S5);  // Set the actual state in the simulator
        SimOutput("Moving ARM machine " + to_string(cpu_type[i].id) + " to S5",
                  4);
        break;
      }
    }
  } else if (amt_in_s3 < s3_threshold) {
    // too few? move up
    SimOutput("Need to move machines from S5 to S3, current S3 count: " +
                  to_string(amt_in_s3) +
                  " threshold: " + to_string(s3_threshold) +
                  " current S5 count " + to_string(amt_in_s5),
              4);
    for (size_t i = 0; i < cpu_type.size(); i++) {
      if (cpu_type[i].s == S5 &&
          Machine_GetInfo(cpu_type[i].id).s_state == S5 &&
          !InMigration(cpu_type[i])) {
        cpu_type[i].s = S3;  // Change to S5
        Machine_SetState(cpu_type[i].id,
                         S3);  // Set the actual state in the simulator
        SimOutput("Moving ARM machine " + to_string(cpu_type[i].id) + " to S0",
                  4);
        break;
      }
    }
  }
}

/* Asynchronously called every so often
   It checks if any tasks still need to be assigned, and assigns them
   Also calls AdjustPercentage() to adjust the percentage of machines in S3 and
   S5 states Finally, it turns off machines that are not in use
*/
void Scheduler::PeriodicCheck(Time_t now) {
  if ((!high_pri.empty() || !mid_pri.empty() || !low_pri.empty())) {
    SimOutput("Still some left", 4);
  }
  AssignTasks();
  TurnOffMachines(mc.arm);
  TurnOffMachines(mc.x86);
  TurnOffMachines(mc.riscv);
  TurnOffMachines(mc.power);

  AdjustPercentage(mc.arm);
  AdjustPercentage(mc.x86);
  AdjustPercentage(mc.riscv);
  AdjustPercentage(mc.power);
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

/* This function is used to compare two machines based on their memory
   utilization. It returns true if the first machine has a lower memory
   utilization than the second machine. Note: ascending order
*/
bool comp(MachineVMs a, MachineVMs b) {
  MachineInfo_t a_info = Machine_GetInfo(a.id);
  MachineInfo_t b_info = Machine_GetInfo(b.id);
  float util_a = (a_info.memory_used * 1.0f) / (a_info.memory_size * 1.0f);
  float util_b = (b_info.memory_used * 1.0f) / (b_info.memory_size * 1.0f);
  return util_a < util_b;
}

/* This function is called when a task is completed.
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
/* This function is called when a state change is complete.
   It assigns any pending tasks to it.
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
  // get correct MachineVMs struct
  size_t i = 0;
  for (i = 0; i < compat_machines->size(); i++) {
    if ((*compat_machines)[i].id == machine_id) {
      break;
    }
  }
  (*compat_machines)[i].s = m_info.s_state;
  // if we put this to sleep, but still have pending tasks, start it back up
  if (m_info.s_state != S0 && pending.find(machine_id) != pending.end()) {
    (*compat_machines)[i].s = S0;
    Machine_SetState(machine_id, S0);
  }
  // otherwise, this machine is now awake, so we can assign pending tasks to it
  else if (m_info.s_state == S0) {
    if (i == compat_machines->size()) {
      (*compat_machines)[i].id = machine_id;
    }

    // exists
    vector<TaskId_t>& tasks = pending[machine_id].tasks;
    // SimOutput("StateChangeComplete: " + to_string(tasks_added) + " tasks
    // added ", 0);
    while (!tasks.empty()) {
      TaskInfo_t task = GetTaskInfo(tasks[0]);
      size_t j = 0;
      // look for eligible VM and add
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