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

void conditionVariableWaitFor(taskr::Runtime &taskr)
{
  // Contention value
  __volatile__ size_t value = 0;

  // Mutex for the condition variable
  taskr::Mutex mutex;

  // Task-aware conditional variable
  taskr::ConditionVariable cv;

  // Time for timeout checking (Microseconds)
  constexpr size_t timeoutTimeUs = 100 * 1000;

  // Forever time to wait (for notification-only waits)
  constexpr size_t forever = 1000 * 1000 * 1000;

  // Creating task functions
  auto waitFc = taskr::Function([&](taskr::Task *task) {
    // Waiting for the other task's notification
    printf("Thread 1: I wait for a notification (Waiting for an hour) \n");
    {
      mutex.lock(task);
      bool wasNotified = cv.waitFor(task, mutex, forever);
      mutex.unlock(task);
      if (wasNotified == false)
      {
        fprintf(stderr, "Error: I have returned do to a timeout!\n");
        exit(1);
      }
      printf("Thread 1: I have been notified (as expected)\n");
    }

    value = 1;

    // Waiting for a timeout
    printf("Thread 1: I wait for a timeout (Waiting for %lums) \n", timeoutTimeUs);
    {
      mutex.lock(task);
      auto startTime   = std::chrono::high_resolution_clock::now();
      bool wasNotified = cv.waitFor(task, mutex, timeoutTimeUs);
      auto currentTime = std::chrono::high_resolution_clock::now();
      auto elapsedTime = (size_t)std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count();
      mutex.unlock(task);
      if (wasNotified == true)
      {
        fprintf(stderr, "Error: I have returned do to a notification!\n");
        exit(1);
      }
      if (elapsedTime < timeoutTimeUs)
      {
        fprintf(stderr, "Error: I have returned earlier than expected!\n");
        exit(1);
      }
      printf("Thread 1: I've exited by timeout (as expected in %luus >= %luus)\n", elapsedTime, timeoutTimeUs);
    }
  });

  auto notifyFc = taskr::Function([&](taskr::Task *task) {
    // Notifying the other task
    printf("Thread 2: Notifying anybody interested (only once)\n");
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
