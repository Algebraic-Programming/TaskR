#include <cstdio>
#include <cassert>
#include <taskr/taskr.hpp>

#define _CONCURRENT_TASKS 1000ul

void mutex(taskr::Runtime *taskr)
{
  // Setting callback to free a task as soon as it finishes executing
  taskr->setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Contention value
  size_t value = 0;

  // Task-aware mutex
  HiCR::tasking::Mutex m;

  // Creating task function
  auto taskfc = taskr::Function([&](taskr::Task *task) {
    m.lock(task);
    value++;
    m.unlock(task);
  });

  // Running concurrent tasks
  for (size_t i = 0; i < _CONCURRENT_TASKS; i++) taskr->addTask(new taskr::Task(i, &taskfc));

  // Initializing TaskR
  taskr->initialize();

  // Running taskR
  taskr->run();

  // Waiting for taskR to finish
  taskr->await();

  // Finalizing TaskR
  taskr->finalize();

  // Value should be equal to concurrent task count
  printf("Value %lu / Expected %lu\n", value, _CONCURRENT_TASKS);
  assert(value == _CONCURRENT_TASKS);
}
