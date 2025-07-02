"""
    Copyright 2025 Huawei Technologies Co., Ltd.
 
  Licensed under the Apache License, Version 2.0 (the "License")
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

def heavyTask(currentTask):
  # Printing starting message
  print(f"Task {currentTask.getLabel()} -- Starting 1 second-long operation.")

  # Getting initial time
  t0 = time.time()

  # Now registering operation
  def operation():
    # Getting current time
    t1 = time.time()

    # Getting difference in ms
    dt = (t1 - t0)*1e3

    # If difference higher than 1 second, the operation is finished
    if dt > 1000: 
      return True

    # Otherwise not
    return False

  # Now registering pending operation
  currentTask.addPendingOperation(operation)

  # Suspending task until the operation is finished
  currentTask.suspend()

  # Printing finished message
  print(f"Task {currentTask.getLabel()} - operation finished")


def pendingOperation(runtime):
  # Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation is finished
  runtime.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, lambda task : runtime.resumeTask(task))

  # Creating the execution units (functions that the tasks will run)
  fc = lambda task : heavyTask(task)

  # Create the taskr Tasks
  taskfc = taskr.Function(fc)

  # Now creating heavy many tasks task
  for i in range(100):
    task = taskr.Task(i, taskfc)

    # Adding to taskr
    runtime.addTask(task)
  
  # Initializing taskr
  runtime.initialize()

  # Running taskr for the current repetition
  runtime.run()

  # Waiting current repetition to end
  runtime.await_()

  # Finalizing taskr
  runtime.finalize()

  # Overwrite the onTaskSuspend fc to be None such that runtime no longer has
  # a dependency to the previous fc and runtime can call the destructor
  runtime.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, None)