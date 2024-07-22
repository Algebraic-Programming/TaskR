#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/runtime.hpp>

#define ITERATIONS 100

void job1(HiCR::backend::host::L1::ComputeManager* computeManager, taskr::Runtime& taskr)
{
  // Creating task functions
  auto taskAfc = computeManager->createExecutionUnit([&taskr]() { printf("Job 1 - Task A %lu\n", taskr.getCurrentTask()->getLabel()); });
  auto taskBfc = computeManager->createExecutionUnit([&taskr]() { printf("Job 1 - Task B %lu\n", taskr.getCurrentTask()->getLabel()); });
  auto taskCfc = computeManager->createExecutionUnit([&taskr]() { printf("Job 1 - Task C %lu\n", taskr.getCurrentTask()->getLabel()); });

  // Now creating tasks and their dependency graph
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto cTask = new HiCR::tasking::Task(i * 3 + 2, taskCfc);
    cTask->addTaskDependency(i * 3 + 1);
    taskr.addTask(cTask);
  }

  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto bTask = new HiCR::tasking::Task(i * 3 + 1, taskBfc);
    bTask->addTaskDependency(i * 3 + 0);
    taskr.addTask(bTask);
  }

  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto aTask = new HiCR::tasking::Task(i * 3 + 0, taskAfc);
    if (i > 0) aTask->addTaskDependency(i * 3 - 1);
    taskr.addTask(aTask);
  }
}
