#include <cstdio>
#include <chrono>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

void heavyTask()
{
  // Printing starting message
  printf("Task %lu -- Starting 1 second-long operation.\n", taskr::getCurrentTask()->getLabel());

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
  taskr::getCurrentTask()->addPendingOperation(operation);

  // Suspending task until the operation is finished
  taskr::getCurrentTask()->suspend();

  // Printing finished message
  printf("Task %lu - operation finished\n", taskr::getCurrentTask()->getLabel());
}

void pendingOperation(taskr::Runtime &taskr)
{
  // Auto-adding task when it suspends. It won't be re-executed until pending operations finish
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { taskr.resumeTask(task); });

  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto taskfc = HiCR::backend::host::L1::ComputeManager::createExecutionUnit([]() { heavyTask(); });

  // Now creating heavy many tasks task
  for (size_t i = 0; i < 100; i++) taskr.addTask(new taskr::Task(i, taskfc));

  // Initializing taskR
  taskr.initialize();

  // Running taskr
  taskr.run();

  // Waiting for taskR to finish
  taskr.await();

  // Finalizing taskR
  taskr.finalize();
}
