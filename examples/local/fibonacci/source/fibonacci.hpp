/*
 *   Copyright 2025 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <chrono>
#include <taskr/taskr.hpp>

static taskr::Runtime       *_taskr;
static std::atomic<uint64_t> _taskCounter;

// Fibonacci without memoization to stress the tasking runtime
uint64_t fibonacci(taskr::Task *currentTask, const uint64_t x)
{
  if (x == 0) return 0;
  if (x == 1) return 1;

  uint64_t result1 = 0;
  uint64_t result2 = 0;
  auto     fibFc1  = taskr::Function([&](taskr::Task *task) { result1 = fibonacci(task, x - 1); });
  auto     fibFc2  = taskr::Function([&](taskr::Task *task) { result2 = fibonacci(task, x - 2); });

  // Creating two new tasks
  taskr::Task subTask1(_taskCounter++, &fibFc1);
  taskr::Task subTask2(_taskCounter++, &fibFc2);

  // Adding dependencies with the newly created tasks
  currentTask->addDependency(&subTask1);
  currentTask->addDependency(&subTask2);

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

  // Storage for result
  uint64_t result = 0;

  // Creating task functions
  auto initialFc = taskr::Function([&](taskr::Task *task) { result = fibonacci(task, initialValue); });

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
