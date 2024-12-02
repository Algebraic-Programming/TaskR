#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <taskr/taskr.hpp>

#define REPETITIONS 2 // 5
#define ITERATIONS 3 // 100

void abcTasks(taskr::Runtime &taskr)
{
  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = taskr::Function([](taskr::Task *task) { printf("Task A %ld\n", task->getLabel()); });
  auto taskBfc = taskr::Function([](taskr::Task *task) { printf("Task B %ld\n", task->getLabel()); });
  auto taskCfc = taskr::Function([](taskr::Task *task) { printf("Task C %ld\n", task->getLabel()); });

  // Initializing taskr
  taskr.initialize();

  // Running the example many times
  for (size_t r = 0; r < REPETITIONS; r++)
  {
    // Calculating the base task id for this repetition
    auto repetitionLabel = r * ITERATIONS * 3;

    // Our connection with the previous iteration is the last task C, null in the first iteration
    taskr::Task *prevTaskC = nullptr;

    // Each run consists of several iterations of ABC
    for (size_t i = 0; i < ITERATIONS; i++)
    {
      auto taskA = new taskr::Task(repetitionLabel + i * 3 + 0, &taskAfc);
      auto taskB = new taskr::Task(repetitionLabel + i * 3 + 1, &taskBfc);
      auto taskC = new taskr::Task(repetitionLabel + i * 3 + 2, &taskCfc);

      // Creating dependencies
      if (i > 0) taskA->addDependency(prevTaskC);
      taskB->addDependency(taskA);
      taskC->addDependency(taskB);

      // Adding to taskr
      taskr.addTask(taskA);
      taskr.addTask(taskB);
      taskr.addTask(taskC);

      // Refreshing previous task C pointer
      prevTaskC = taskC;
    }

    // Running taskr for the current repetition
    taskr.run();

    // Waiting current repetition to end
    taskr.await();
  }

  // Finalizing taskr
  taskr.finalize();
}
