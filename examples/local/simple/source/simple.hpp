#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <taskr/taskr.hpp>

void simple(taskr::Runtime *taskr)
{
  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr->setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Initializing taskr
  taskr->initialize();

  auto fc = [](taskr::Task *task) { printf("Hello, I am task %ld\n", task->getLabel()); };

  // Create the taskr Tasks
  auto taskfc = taskr::Function(fc);

  // Creating the execution units (functions that the tasks will run)
  for (int i = 0; i < 1; ++i)
  {
    auto task = new taskr::Task(i, &taskfc);

    // Adding to taskr
    taskr->addTask(task);
  }

  // Running taskr for the current repetition
  taskr->run();

  // Waiting current repetition to end
  taskr->await();

  // Finalizing taskr
  taskr->finalize();
}
