#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/runtime.hpp>

#define ITERATIONS 10

void abcTasks(HiCR::backend::host::L1::ComputeManager *computeManager, const HiCR::L0::Device::computeResourceList_t &computeResources)
{
  // Initializing taskr
  taskr::Runtime taskr;

  // Assigning processing Re to TaskR
  for (const auto &computeResource : computeResources) taskr.addProcessingUnit(computeManager->createProcessingUnit(computeResource));

  // Storage for the tasks we'll create
  std::vector<HiCR::tasking::Task*> tasks(3 * ITERATIONS);

  // Now creating tasks
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    size_t taskId = i * 3 + 2;
    auto taskfc = computeManager->createExecutionUnit([taskId]() { printf("Task C %lu\n", taskId); });
    tasks[taskId] = new HiCR::tasking::Task(taskfc);
  }

  for (size_t i = 0; i < ITERATIONS; i++)
  {
    size_t taskId = i * 3 + 1;
    auto taskfc = computeManager->createExecutionUnit([taskId]() { printf("Task B %lu\n", taskId); });
    tasks[taskId] = new HiCR::tasking::Task(taskfc);
  }

  for (size_t i = 0; i < ITERATIONS; i++)
  {
    size_t taskId = i * 3 + 0;
    auto taskfc = computeManager->createExecutionUnit([taskId]() { printf("Task A %lu\n", taskId); });
    tasks[taskId] = new HiCR::tasking::Task(taskfc);
  }

  // Now creating the dependency graph
  for (size_t i = 0; i < ITERATIONS; i++) taskr.addTaskDependency(tasks[i * 3 + 2], tasks[i * 3 + 1]);
  for (size_t i = 0; i < ITERATIONS; i++) taskr.addTaskDependency(tasks[i * 3 + 1], tasks[i * 3 + 0]);
  for (size_t i = 0; i < ITERATIONS; i++) if (i > 0) taskr.addTaskDependency(tasks[i * 3 + 0], tasks[i * 3 - 1]);

  // Adding tasks to TaskR
  for (const auto task : tasks) taskr.addTask(task);

  // Running taskr
  taskr.run(computeManager);
}
