#include <cstdio>
#include <cassert>
#include <thread>
#include <chrono>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/runtime.hpp>

using namespace std::chrono_literals;
#define _INITIAL_VALUE 7ul

void conditionVariable(taskr::Runtime& taskr)
{
  // Auto-adding task when it receives a sync signal
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSync, [&](taskr::Task* task) { taskr.resumeTask(task); });

  // Contention value
  size_t value = 0;

  // Mutex for the condition variable
  HiCR::tasking::Mutex mutex;

  // Task-aware conditional variable
  HiCR::tasking::ConditionVariable cv;

  // Creating task functions
  auto thread1Fc = HiCR::backend::host::L1::ComputeManager::createExecutionUnit([&]() {
    // Using lock to update the value
    mutex.lock();
    printf("Thread 1: I go first and set value to 1\n");
    value += 1;
    mutex.unlock();

    // Notifiying the other thread
    printf("Thread 1: Now I notify anybody waiting\n");
    cv.notifyOne();

    // Waiting for the other thread's update now
    printf("Thread 1: I wait for the value to turn 2\n");
    cv.wait(mutex, [&]() { return value == 2; });
    printf("Thread 1: The condition (value == 2) is satisfied now\n");
  });

  auto thread2Fc = HiCR::backend::host::L1::ComputeManager::createExecutionUnit([&]() {
    // Waiting for the other thread to set the first value
    printf("Thread 2: First, I'll wait for the value to become 1\n");
    cv.wait(mutex, [&]() { return value == 1; });
    printf("Thread 2: The condition (value == 1) is satisfied now\n");

    // Now updating the value ourselves
    printf("Thread 2: Now I update the value to 2\n");
    mutex.lock();
    value += 1;
    mutex.unlock();

    // Notifying the other thread
    printf("Thread 2: Notifying anybody interested\n");
    cv.notifyOne();
  });

  taskr::Task task1(0, thread1Fc);
  taskr::Task task2(1, thread2Fc);

  taskr.addTask(&task1);
  taskr.addTask(&task2);

  // Initializing taskr
  taskr.initialize();

  // Running taskr
  taskr.run();

  // Finalizing taskr
  taskr.finalize();

  // Value should be equal to concurrent task count
  size_t expectedValue = 2;
  printf("Value %lu / Expected %lu\n", value, expectedValue);
  assert(value == expectedValue);
}
