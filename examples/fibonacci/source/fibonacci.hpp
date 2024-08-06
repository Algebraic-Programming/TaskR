#include <cstdio>
#include <chrono>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

static HiCR::backend::host::L1::ComputeManager *_computeManager;
static taskr::Runtime                          *_taskr;
static std::atomic<uint64_t>                    _taskCounter;

// Fibonacci without memoization to stress the tasking runtime
uint64_t fibonacci(const uint64_t x)
{
  if (x == 0) return 0;
  if (x == 1) return 1;

  uint64_t result1 = 0;
  uint64_t result2 = 0;
  auto     fibFc1  = _computeManager->createExecutionUnit([&]() { result1 = fibonacci(x - 1); });
  auto     fibFc2  = _computeManager->createExecutionUnit([&]() { result2 = fibonacci(x - 2); });

  // Creating two new tasks
  taskr::Task subTask1(_taskCounter++, fibFc1);
  taskr::Task subTask2(_taskCounter++, fibFc2);

  // Getting the current task
  auto currentTask = taskr::getCurrentTask();

  // Adding dependencies with the newly created tasks
  _taskr->addDependency(currentTask, &subTask1);
  _taskr->addDependency(currentTask, &subTask2);

  // Adding new tasks to TaskR
  _taskr->addTask(&subTask1);
  _taskr->addTask(&subTask2);

  // Suspending current task
  currentTask->suspend();

  return result1 + result2;
}

uint64_t fibonacciDriver(const uint64_t initialValue, HiCR::backend::host::L1::ComputeManager *computeManager, const HiCR::L0::Device::computeResourceList_t &computeResources)
{
  // Initializing taskr with the appropriate amount of max tasks
  taskr::Runtime taskr;

  // Setting global variables
  _taskr          = &taskr;
  _computeManager = computeManager;
  _taskCounter    = 0;

  // Assigning processing resource to TaskR
  for (const auto &computeResource : computeResources) taskr.addProcessingUnit(computeManager->createProcessingUnit(computeResource));

  // Storage for result
  uint64_t result = 0;

  // Creating task functions
  auto initialFc = computeManager->createExecutionUnit([&]() { result = fibonacci(initialValue); });

  // Now creating tasks and their dependency graph
  taskr::Task initialTask(_taskCounter++, initialFc);
  taskr.addTask(&initialTask);

  // Running taskr
  auto startTime = std::chrono::high_resolution_clock::now();
  taskr.run(computeManager);
  auto endTime     = std::chrono::high_resolution_clock::now();
  auto computeTime = std::chrono::duration_cast<std::chrono::duration<double>>(endTime - startTime);
  printf("Running Time: %0.5fs\n", computeTime.count());
  printf("Total Tasks: %lu\n", _taskCounter.load());

  // Returning fibonacci value
  return result;
}
