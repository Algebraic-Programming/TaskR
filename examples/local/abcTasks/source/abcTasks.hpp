#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

#define ITERATIONS 100

void abcTasks(HiCR::backend::host::L1::ComputeManager *computeManager, const HiCR::L0::Device::computeResourceList_t &computeResources)
{
  // Initializing taskr
  taskr::Runtime taskr;

  // Assigning processing resources to TaskR
  for (const auto &computeResource : computeResources) taskr.addProcessingUnit(computeManager->createProcessingUnit(computeResource));

  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Creating a storage for all the tasks we will create in this example
  std::vector<taskr::Task *> tasks(3 * ITERATIONS);

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = computeManager->createExecutionUnit([]() { printf("Task A %lu\n", (taskr::getCurrentTask())->getLabel()); });
  auto taskBfc = computeManager->createExecutionUnit([]() { printf("Task B %lu\n", (taskr::getCurrentTask())->getLabel()); });
  auto taskCfc = computeManager->createExecutionUnit([]() { printf("Task C %lu\n", (taskr::getCurrentTask())->getLabel()); });

  // Now creating tasks
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto taskId   = i * 3 + 1;
    tasks[taskId] = new taskr::Task(taskId, taskBfc);
  }
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto taskId   = i * 3 + 0;
    tasks[taskId] = new taskr::Task(taskId, taskAfc);
  }
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto taskId   = i * 3 + 2;
    tasks[taskId] = new taskr::Task(taskId, taskCfc);
  }

  // Now creating the dependency graph
  for (size_t i = 0; i < ITERATIONS; i++) tasks[i * 3 + 2]->addDependency(i * 3 + 1);
  for (size_t i = 0; i < ITERATIONS; i++) tasks[i * 3 + 1]->addDependency(i * 3 + 0);
  for (size_t i = 1; i < ITERATIONS; i++) tasks[i * 3 + 0]->addDependency(i * 3 - 1);

  // Adding tasks to taskr
  for (const auto task : tasks) taskr.addTask(task);

  // Running taskr
  taskr.run(computeManager);
}
