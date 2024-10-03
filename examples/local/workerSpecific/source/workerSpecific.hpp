#include <cstdio>
#include <sched.h>
#include <hicr/core/L0/device.hpp>
#include <taskr/taskr.hpp>

void workFc()
{
  auto currentTask  = taskr::getCurrentTask();
  auto taskLabel    = currentTask->getLabel();
  int  currentCPUId = sched_getcpu();

  ////// First launched on even cpus

  printf("Task %lu first run running on CPU %d\n", taskLabel, currentCPUId);

  // Sanity check
  if ((int)taskLabel != currentCPUId)
  {
    fprintf(stderr, "Task label (%lu) does not coincide with the current CPU id! (%d)\n", taskLabel, currentCPUId);
    std::abort();
  }

  // Changing to odd cpus
  currentTask->setWorkerAffinity(currentTask->getWorkerAffinity() + 1);

  // Suspending
  currentTask->suspend();

  ///// Now launched in odd cpus

  currentCPUId = sched_getcpu();
  printf("Task %lu second run running on CPU %d\n", taskLabel, currentCPUId);

  // Sanity check
  if ((int)taskLabel + 1 != currentCPUId)
  {
    fprintf(stderr, "Task label (%lu) + 1 does not coincide with the current CPU id! (%d)\n", taskLabel, currentCPUId);
    std::abort();
  }
}

void workerSpecific(taskr::Runtime &taskr, const size_t workerCount)
{
  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Auto-adding task when it suspends.
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { taskr.resumeTask(task); });

  // Creating the execution units (functions that the tasks will run)
  auto workTaskfc = taskr::Function([](taskr::Task *task) { workFc(); });

  // Initializing taskr
  taskr.initialize();

  // Run only on even worker ids
  for (size_t i = 0; i < workerCount; i++)
    if (i % 2 == 0) taskr.addTask(new taskr::Task(i, &workTaskfc, i));

  // Running taskr for the current repetition
  taskr.run();

  // Waiting for taskr to finish
  taskr.await();

  // Finalizing taskr
  taskr.finalize();
}
