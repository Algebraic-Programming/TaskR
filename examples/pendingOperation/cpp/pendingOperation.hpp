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
#include <hicr/core/device.hpp>
#include <hicr/backends/pthreads/computeManager.hpp>
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
  // Setting onTaskFinish callback to free up task's memory after it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation is finished
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&taskr](taskr::Task *task) { taskr.resumeTask(task); });

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
