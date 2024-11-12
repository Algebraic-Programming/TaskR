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

  // Creating task functions
  auto waitFc = taskr::Function([&](taskr::Task *task) {

    // Waiting for the other task's notification
    printf("Thread 1: I wait for a notification (Waiting for an hour) \n");
    {
      mutex.lock(task);
      bool wasNotified = cv.waitFor(task, mutex, 3600000);
      mutex.unlock(task);
      if (wasNotified == false) { fprintf(stderr, "Error: I have returned do to a timeout!\n"); exit(1); }
      printf("Thread 1: I have been notified (as expected)\n");
    }

    value = 1;
    
    // Waiting for a timeout
    printf("Thread 1: I wait for a timeout (Waiting for 10ms) \n");
    {
      mutex.lock(task);
      bool wasNotified = cv.waitFor(task, mutex, 10);
      mutex.unlock(task);
      if (wasNotified == true) { fprintf(stderr, "Error: I have returned do to a nofitication!\n"); exit(1); }
      printf("Thread 1: I've exited by timeout (as expected)\n");
    }

  });

  auto notifyFc = taskr::Function([&](taskr::Task *task) {
    // Notifying the other task
    printf("Thread 2: Notifying anybody interested (only once)\n");
    while (value != 1) { cv.notifyOne(task); task->suspend(); }
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
