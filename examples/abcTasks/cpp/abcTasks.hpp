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
#include <hicr/core/device.hpp>
#include <taskr/taskr.hpp>

#define REPETITIONS 5
#define ITERATIONS 100

void abcTasks(taskr::Runtime &taskr)
{
  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = taskr::Function(taskr.getTaskComputeManager(), [](taskr::Task *task) { printf("Task A %ld\n", task->getTaskId()); });
  auto taskBfc = taskr::Function(taskr.getTaskComputeManager(), [](taskr::Task *task) { printf("Task B %ld\n", task->getTaskId()); });
  auto taskCfc = taskr::Function(taskr.getTaskComputeManager(), [](taskr::Task *task) { printf("Task C %ld\n", task->getTaskId()); });

  // Initializing taskr
  taskr.initialize();

  // Running the example many times
  for (size_t r = 0; r < REPETITIONS; r++)
  {
    // Calculating the base task id for this repetition
    auto repetitionTaskId = r * ITERATIONS * 3;

    // Our connection with the previous iteration is the last task C, null in the first iteration
    taskr::Task *prevTaskC = nullptr;

    // Each run consists of several iterations of ABC
    for (size_t i = 0; i < ITERATIONS; i++)
    {
      auto taskA = new taskr::Task(repetitionTaskId + i * 3 + 0, &taskAfc);
      auto taskB = new taskr::Task(repetitionTaskId + i * 3 + 1, &taskBfc);
      auto taskC = new taskr::Task(repetitionTaskId + i * 3 + 2, &taskCfc);

      // Creating dependencies
      if (i > 0) taskA->addDependency(prevTaskC);
      taskB->addDependency(taskA);
      taskC->addDependency(taskB);

      // Adding to taskr
      taskr.addTask(taskA);
      taskr.addTask(taskB);
      taskr.addTask(taskC);

      // Refreshing previous task C pointer
      prevTaskC = taskC;
    }

    // Running taskr for the current repetition
    taskr.run();

    // Waiting current repetition to end
    taskr.await();
  }

  // Finalizing taskr
  taskr.finalize();
}
