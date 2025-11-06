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
#include <cassert>
#include <thread>
#include <chrono>
#include <taskr/taskr.hpp>

void conditionVariableWait(taskr::Runtime &taskr)
{
  // Contention value
  __volatile__ size_t value = 0;

  // Mutex for the condition variable
  taskr::Mutex mutex;

  // Task-aware conditional variable
  taskr::ConditionVariable cv;

  // Creating task functions
  auto waitFc = taskr::Function(taskr.getTaskComputeManager(), [&](taskr::Task *task) {
    // Waiting for the other task's notification
    printf("Thread 1: I wait for a notification\n");
    mutex.lock(task);
    cv.wait(task, mutex);
    mutex.unlock(task);
    value = 1;
    printf("Thread 1: I have been notified\n");
  });

  auto notifyFc = taskr::Function(taskr.getTaskComputeManager(), [&](taskr::Task *task) {
    // Notifying the other task
    printf("Thread 2: Notifying anybody interested\n");
    while (value != 1)
    {
      cv.notifyOne(task);
      task->suspend();
    }
  });

  taskr::Task task1(0, &waitFc);
  taskr::Task task2(1, &notifyFc);

  taskr.addTask(&task1);
  taskr.addTask(&task2);

  // Initializing taskr
  taskr.initialize();

  // Running taskr
  taskr.run();

  // Waiting for task to finish
  taskr.await();

  // Finalizing taskr
  taskr.finalize();
}
