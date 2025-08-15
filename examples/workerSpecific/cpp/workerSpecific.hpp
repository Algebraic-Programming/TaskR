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
#include <sched.h>
#include <hicr/core/device.hpp>
#include <taskr/taskr.hpp>

void workFc(taskr::Task *currentTask)
{
  auto taskId       = currentTask->getTaskId();
  int  currentCPUId = sched_getcpu();

  ////// First launched on even cpus

  printf("Task %lu first run running on CPU %d\n", taskId, currentCPUId);

  // Sanity check
  if (int(2 * taskId) != currentCPUId)
  {
    fprintf(stderr, "Task ID (%lu) does not coincide with the current CPU id! (%d)\n", taskId, currentCPUId);
    std::abort();
  }

  // Changing to odd cpus
  currentTask->setWorkerAffinity(currentTask->getWorkerAffinity() + 1);

  // Suspending
  currentTask->suspend();

  ///// Now launched in odd cpus

  currentCPUId = sched_getcpu();
  printf("Task %lu second run running on CPU %d\n", taskId, currentCPUId);

  // Sanity check
  if (int(2 * taskId) + 1 != currentCPUId)
  {
    fprintf(stderr, "Task ID (%lu) + 1 does not coincide with the current CPU id! (%d)\n", taskId, currentCPUId);
    std::abort();
  }
}

void workerSpecific(taskr::Runtime &taskr, const size_t workerCount)
{
  // Setting callback to free a task as soon as it finishes executing
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Auto-adding task when it suspends.
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { taskr.resumeTask(task); });

  // Creating the execution units (functions that the tasks will run)
  auto workTaskfc = taskr::Function([](taskr::Task *task) { workFc(task); });

  // Initializing taskr
  taskr.initialize();

  const size_t half_workerCount = (size_t)workerCount / 2;

  // Run only on even worker ids
  for (size_t i = 0; i < half_workerCount; i++) taskr.addTask(new taskr::Task(i, &workTaskfc, 2 * i));

  // Running taskr for the current repetition
  taskr.run();

  // Waiting for taskr to finish
  taskr.await();

  // Finalizing taskr
  taskr.finalize();
}
