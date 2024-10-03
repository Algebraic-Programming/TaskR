#include <cstdio>
#include <chrono>
#include <taskr/taskr.hpp>

static taskr::Runtime       *_taskr;
static std::atomic<uint64_t> _taskCounter;

// Fibonacci without memoization to stress the tasking runtime
uint64_t fibonacci(const uint64_t x)
{
  if (x == 0) return 0;
  if (x == 1) return 1;

  uint64_t result1 = 0;
  uint64_t result2 = 0;
  auto     fibFc1  = taskr::Function([&](taskr::Task* task) { result1 = fibonacci(x - 1); });
  auto     fibFc2  = taskr::Function([&](taskr::Task* task) { result2 = fibonacci(x - 2); });

  // Creating two new tasks
  taskr::Task subTask1(_taskCounter++, &fibFc1);
  taskr::Task subTask2(_taskCounter++, &fibFc2);

  // Getting the current task
  auto currentTask = taskr::getCurrentTask();

  // Adding dependencies with the newly created tasks
  currentTask->addDependency(subTask1.getLabel());
  currentTask->addDependency(subTask2.getLabel());

  // Adding new tasks to TaskR
  _taskr->addTask(&subTask1);
  _taskr->addTask(&subTask2);

  // Suspending current task
  currentTask->suspend();

  return result1 + result2;
}

uint64_t fibonacciDriver(const uint64_t initialValue, taskr::Runtime &taskr)
{
  // Setting global variables
  _taskr       = &taskr;
  _taskCounter = 0;

  // Auto-adding task upon suspend, to allow it to run as soon as it dependencies have been satisfied
  _taskr->setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { _taskr->resumeTask(task); });

  // Storage for result
  uint64_t result = 0;

  // Creating task functions
  auto initialFc = taskr::Function([&](taskr::Task* task) { result = fibonacci(initialValue); });

  // Now creating tasks and their dependency graph
  taskr::Task initialTask(_taskCounter++, &initialFc);
  taskr.addTask(&initialTask);

  // Initializing taskR
  taskr.initialize();

  // Running taskr
  auto startTime = std::chrono::high_resolution_clock::now();
  taskr.run();
  taskr.await();
  auto endTime     = std::chrono::high_resolution_clock::now();
  auto computeTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
  printf("Running Time: %0.5fs\n", computeTime.count());
  printf("Total Tasks: %lu\n", _taskCounter.load());

  // Finalizing taskR
  taskr.finalize();

  // Returning fibonacci value
  return result;
}
