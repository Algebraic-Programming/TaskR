#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <taskr/taskr.hpp>

#define NSUSPENDS 1000

void suspend(taskr::Runtime &taskr, const size_t branchCount, const size_t taskCount)
{
  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation is finished
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&taskr](taskr::Task *task) { taskr.resumeTask(task); });

  // Creating the execution units (functions that the tasks will run)
  auto taskfc = taskr::Function([](taskr::Task *task) {
    for (size_t i = 0; i < NSUSPENDS; i++) task->suspend();
  });

  // Initializing taskr
  taskr.initialize();

  // Store a pointer to the previous task to generate a long chain
  taskr::Task *prevTask = nullptr;

  // Each run consists of several iterations of ABC
  for (size_t b = 0; b < branchCount; b++)
    for (size_t i = 0; i < taskCount; i++)
    {
      auto task = new taskr::Task(b * taskCount + i, &taskfc);

      // Creating dependencies
      if (i > 0) task->addDependency(prevTask);

      // Adding to taskr
      taskr.addTask(task);

      // Setting as new previous task
      prevTask = task;
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
