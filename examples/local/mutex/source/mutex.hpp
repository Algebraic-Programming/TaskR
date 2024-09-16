#include <cstdio>
#include <cassert>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

#define _CONCURRENT_TASKS 1000ul

void mutex(taskr::Runtime* taskr)
{
  // Setting callback to free a task as soon as it finishes executing
  taskr->setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });
  
  // Auto-adding task when it receives a sync signal
  taskr->setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSync, [&](taskr::Task* task) { taskr->resumeTask(task); });
  
  // Contention value
  size_t value = 0;

  // Task-aware mutex
  HiCR::tasking::Mutex m;

  // Creating task function
  auto taskfc = HiCR::backend::host::L1::ComputeManager::createExecutionUnit([&]() {
    m.lock();
    value++;
    m.unlock();
  });

  // Running concurrent tasks
  for (size_t i = 0; i < _CONCURRENT_TASKS; i++) taskr->addTask(new taskr::Task(i, taskfc));

  // Initializing TaskR
  taskr->initialize();

  // Running taskR
  taskr->run();

  // Finalizing TaskR
  taskr->finalize();

  // Value should be equal to concurrent task count
  printf("Value %lu / Expected %lu\n", value, _CONCURRENT_TASKS);
  assert(value == _CONCURRENT_TASKS);
}
