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

void conditionVariableWaitForCondition(taskr::Runtime &taskr)
{
  // Contention value
  size_t value = 0;

  // Mutex for the condition variable
  taskr::Mutex mutex;

  // Task-aware conditional variable
  taskr::ConditionVariable cv;

  // Time for timeout checking (Microseconds)
  constexpr size_t timeoutTimeUs = 100 * 1000;

  // Forever time to wait
  constexpr size_t forever = 1000 * 1000 * 1000;

  // Creating task functions
  auto thread1Fc = taskr::Function([&](taskr::Task *task) {
    // Using lock to update the value
    printf("Thread 1: I go first and set value to 1\n");
    mutex.lock(task);
    value = 1;
    mutex.unlock(task);

    // Notifiying the other thread
    printf("Thread 1: Now I notify anybody waiting\n");
    while (value != 2)
    {
      cv.notifyOne(task);
      task->suspend();
    }

    // Waiting for the other thread's update now
    {
      printf("Thread 1: I wait (forever) for the value to turn 2\n");
      mutex.lock(task);
      bool wasNotified = cv.waitFor(
        task, mutex, [&]() { return value == 2; }, forever);
      mutex.unlock(task);
      if (wasNotified == false)
      {
        fprintf(stderr, "Error: I have returned do to a timeout!\n");
        exit(1);
      }
      printf("Thread 1: The condition (value == 2) is satisfied now\n");
    }

    // Now waiting for a condition that won't be met, although we'll get notifications
    {
      printf("Thread 1: I wait (with timeout) for the value to turn 3 (won't happen)\n");
      mutex.lock(task);
      auto startTime   = std::chrono::high_resolution_clock::now();
      bool wasNotified = cv.waitFor(
        task, mutex, [&]() { return value == 3; }, timeoutTimeUs);
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

    // Updating value to 3 now, to release the other thread
    mutex.lock(task);
    value = 3;
    mutex.unlock(task);
  });

  auto thread2Fc = taskr::Function([&](taskr::Task *task) {
    // Waiting for the other thread to set the first value
    printf("Thread 2: First, I'll wait for the value to become 1\n");
    mutex.lock(task);
    bool wasNotified = cv.waitFor(
      task, mutex, [&]() { return value == 1; }, forever);
    mutex.unlock(task);
    if (wasNotified == false)
    {
      fprintf(stderr, "Error: I have returned do to a timeout!\n");
      exit(1);
    }
    printf("Thread 2: The condition (value == 1) is satisfied now\n");

    // Now updating the value ourselves
    printf("Thread 2: Now I update the value to 2\n");
    mutex.lock(task);
    value = 2;
    mutex.unlock(task);

    // Notifying the other thread
    printf("Thread 2: Notifying constantly until the value is 3\n");
    while (value != 3)
    {
      cv.notifyOne(task);
      task->suspend();
    }
  });

  taskr::Task task1(0, &thread1Fc);
  taskr::Task task2(1, &thread2Fc);

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

  // Value should be equal to concurrent task count
  constexpr size_t expectedValue = 3;
  printf("Value %lu / Expected %lu\n", value, expectedValue);
  assert(value == expectedValue);
}
