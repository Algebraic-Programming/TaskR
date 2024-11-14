#include <cstdio>
#include <cassert>
#include <thread>
#include <chrono>
#include <taskr/taskr.hpp>

void conditionVariableWaitCondition(taskr::Runtime &taskr)
{
  // Contention value
  size_t value = 0;

  // Mutex for the condition variable
  taskr::Mutex mutex;

  // Task-aware conditional variable
  taskr::ConditionVariable cv;

  // Creating task functions
  auto thread1Fc = taskr::Function([&](taskr::Task *task) {
    // Using lock to update the value
    mutex.lock(task);
    printf("Thread 1: I go first and set value to 1\n");
    value++;
    mutex.unlock(task);

    // Notifiying the other thread
    printf("Thread 1: Now I notify anybody waiting\n");
    while (value != 1)
    {
      cv.notifyOne(task);
      task->suspend();
    }

    // Waiting for the other thread's update now
    printf("Thread 1: I wait for the value to turn 2\n");
    mutex.lock(task);
    cv.wait(task, mutex, [&]() { return value == 2; });
    mutex.unlock(task);
    printf("Thread 1: The condition (value == 2) is satisfied now\n");
  });

  auto thread2Fc = taskr::Function([&](taskr::Task *task) {
    // Waiting for the other thread to set the first value
    printf("Thread 2: First, I'll wait for the value to become 1\n");
    mutex.lock(task);
    cv.wait(task, mutex, [&]() { return value == 1; });
    mutex.unlock(task);
    printf("Thread 2: The condition (value == 1) is satisfied now\n");

    // Now updating the value ourselves
    printf("Thread 2: Now I update the value to 2\n");
    mutex.lock(task);
    value++;
    mutex.unlock(task);

    // Notifying the other thread
    printf("Thread 2: Notifying anybody interested\n");
    cv.notifyOne(task);
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
  size_t expectedValue = 2;
  printf("Value %lu / Expected %lu\n", value, expectedValue);
  assert(value == expectedValue);
}
