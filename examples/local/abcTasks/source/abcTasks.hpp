#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

#define REPETITIONS 5
#define ITERATIONS 100

void abcTasks(HiCR::backend::host::L1::ComputeManager *computeManager, const HiCR::L0::Device::computeResourceList_t &computeResources)
{
  // Creating taskr
  taskr::Runtime taskr(computeManager);

  // Assigning processing resources to TaskR
  for (const auto &computeResource : computeResources) taskr.addProcessingUnit(computeManager->createProcessingUnit(computeResource));

  // Initializing taskr
  taskr.initialize();

  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = computeManager->createExecutionUnit([]() { printf("Task A %lu\n", (taskr::getCurrentTask())->getLabel()); });
  auto taskBfc = computeManager->createExecutionUnit([]() { printf("Task B %lu\n", (taskr::getCurrentTask())->getLabel()); });
  auto taskCfc = computeManager->createExecutionUnit([]() { printf("Task C %lu\n", (taskr::getCurrentTask())->getLabel()); });

  // Running the example many times
  for (size_t r = 0; r < REPETITIONS; r++)
  {
    // Calculating the base task id for this repetition
    taskr::Task::label_t repetitionLabel = r * ITERATIONS * 3;

    // Each run consists of several iterations of ABC
    for (size_t i = 0; i < ITERATIONS; i++)
    {
      auto taskB = new taskr::Task(repetitionLabel + i * 3 + 1, taskBfc);
      auto taskA = new taskr::Task(repetitionLabel + i * 3 + 0, taskAfc);
      auto taskC = new taskr::Task(repetitionLabel + i * 3 + 2, taskCfc);

      // Creating dependencies
      if (i > 0) taskA->addDependency(repetitionLabel + i * 3 - 1);
      taskB->addDependency(repetitionLabel + i * 3 + 0);
      taskC->addDependency(repetitionLabel + i * 3 + 1);

      // Adding to taskr
      taskr.addTask(taskA);
      taskr.addTask(taskB);
      taskr.addTask(taskC);
    }

    // Running taskr
    taskr.run();
  }

  // Finalizing taskr
  taskr.finalize();
}
