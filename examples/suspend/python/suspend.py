"""
    Copyright 2025 Huawei Technologies Co., Ltd.
 
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
 
      http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
"""

import time

import taskr

NSUSPENDS = 1000

def suspend(runtime, branchCount, taskCount):
  # Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation is finished
  runtime.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, lambda task : runtime.resumeTask(task))

  def fc(task):
    for _ in range(NSUSPENDS): task.suspend()

  # Creating the execution units (functions that the tasks will run)
  taskfc = taskr.Function(fc)

  # Initializing taskr
  runtime.initialize()

  # Creating the execution units (functions that the tasks will run)
  prevTask = None
  for b in range(branchCount):
    for i in range(taskCount):
      task = taskr.Task(b * taskCount + i, taskfc)

      # Creating dependencies
      if i > 0: task.addDependency(prevTask)

      # Adding to taskr
      runtime.addTask(task)

      # Setting as new previous task
      prevTask = task

  # Running taskr for the current repetition
  startTime = time.time()
  runtime.run()
  runtime.wait()

  endTime = time.time()
  computeTime = endTime - startTime

  print(f"Running Time: {computeTime:0.5f}s")

  # Finalizing taskr
  runtime.finalize()

  # Overwrite the onTaskSuspend fc to be None such that runtime no longer has
  # a dependency to the previous fc and runtime can call the destructor
  runtime.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, None)