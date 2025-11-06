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

void simple(taskr::Runtime *taskr)
{
  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr->setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Initializing taskr
  taskr->initialize();

  auto fc = [](taskr::Task *task) { printf("Hello, I am task %ld\n", task->getTaskId()); };

  // Create the taskr Tasks
  auto taskfc = taskr::Function(taskr->getTaskComputeManager(), fc);

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
