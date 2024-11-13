#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <taskr/taskr.hpp>

void manyParallel(taskr::Runtime &taskr, const size_t branchCount, const size_t taskCount)
{
  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto taskfc = taskr::Function([](taskr::Task *task) {});

  // Initializing taskr
  taskr.initialize();

  // Each run consists of several iterations of ABC
  for (size_t b = 0; b < branchCount; b++)
    for (size_t i = 0; i < taskCount; i++)
    {
      auto task = new taskr::Task(b * taskCount + i, &taskfc);

      // Creating dependencies
      if (i > 0) taskr.addDependency(task, b * taskCount + i - 1);

      // Adding to taskr
      taskr.addTask(task);
    }

  // Running taskr for the current repetition
  auto startTime = std::chrono::high_resolution_clock::now();
  taskr.run();
  taskr.await();

  // Getting running time
  auto endTime     = std::chrono::high_resolution_clock::now();
  auto computeTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
  printf("Running Time: %0.5fs\n", computeTime.count());

  // Finalizing taskr
  taskr.finalize();
}
