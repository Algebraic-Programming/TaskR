#include <cstdio>
#include <chrono>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

void heavyTask(taskr::Task *currentTask)
{
  // Printing starting message
  printf("Task %lu -- Starting 1 second-long operation.\n", currentTask->getLabel());

  // Getting initial time
  auto t0 = std::chrono::high_resolution_clock::now();

  // Now registering operation
  auto operation = [t0]() {
    // Getting current time
    auto t1 = std::chrono::high_resolution_clock::now();

    // Getting difference in ms
    auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    // If difference higher than 1 second, the operation is finished
    if (dt > 1000) return true;

    // Otherwise not
    return false;
  };

  // Now registering pending operation
  currentTask->addPendingOperation(operation);

  // Suspending task until the operation is finished
  currentTask->suspend();

  // Printing finished message
  printf("Task %lu - operation finished\n", currentTask->getLabel());
}

void pendingOperation(taskr::Runtime &taskr)
{
  // Auto-adding task when it suspends. It won't be re-executed until pending operations finish
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { taskr.resumeTask(task); });

  // Setting onTaskFinish callback to free up task's memory after it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto taskfc = taskr::Function([](taskr::Task *task) { heavyTask(task); });

  // Now creating heavy many tasks task
  for (size_t i = 0; i < 100; i++) taskr.addTask(new taskr::Task(i, &taskfc));

  // Initializing taskR
  taskr.initialize();

  // Running taskr
  taskr.run();

  // Waiting for taskR to finish
  taskr.await();

  // Finalizing taskR
  taskr.finalize();
}
