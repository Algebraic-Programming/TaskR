#include <cstdio>
#include <sched.h>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

void workFc()
{
  auto taskLabel    = taskr::getCurrentTask()->getLabel();
  int  currentCPUId = sched_getcpu();

  printf("Task %lu running on CPU %d\n", taskLabel, currentCPUId);

  // Sanity check
  if ((int)taskLabel != currentCPUId)
  {
    fprintf(stderr, "Task label (%lu) does not coincide with the current CPU id! (%d)\n", taskLabel, currentCPUId);
    std::abort();
  }
}

void workerSpecific(taskr::Runtime &taskr, const size_t workerCount)
{
  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto workTaskfc = HiCR::backend::host::L1::ComputeManager::createExecutionUnit([]() { workFc(); });

  // Initializing taskr
  taskr.initialize();

  // Run only on even worker ids
  for (size_t i = 0; i < workerCount; i++)
    if (i % 2 == 0) taskr.addTask(new taskr::Task(i, workTaskfc, i));

  // Running taskr for the current repetition
  taskr.run();

  // Finalizing taskr
  taskr.finalize();
}
