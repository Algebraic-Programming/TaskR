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

import ctypes

libc = ctypes.CDLL("libc.so.6")
libc.sched_getcpu.restype = ctypes.c_int

import sys

import taskr

def workFc(currentTask):
  taskLabel = currentTask.getLabel()
  currentCPUId = libc.sched_getcpu()

  #### First launched on even cpus

  print(f"Task {taskLabel} first run running on CPU {currentCPUId}")

  # Sanity check
  if int(2 * taskLabel) != currentCPUId:
    sys.stderr.write(f"Task label ({taskLabel}) does not coincide with the current CPU id! ({currentCPUId})")
    sys.exit(1)

  # Changing to odd cpus
  currentTask.setWorkerAffinity(currentTask.getWorkerAffinity() + 1)

  # Suspending
  currentTask.suspend()

  #### Now launched in odd cpus

  currentCPUId = libc.sched_getcpu()
  print(f"Task {taskLabel} second run running on CPU {currentCPUId}")

  # Sanity check
  if int(2 * taskLabel) + 1 != currentCPUId:
    sys.stderr.write(f"Task label ({taskLabel}) + 1 does not coincide with the current CPU id! ({currentCPUId})")
    sys.exit(1)


def workerSpecific(runtime, workerCount):
  # Auto-adding task when it suspends.
  runtime.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, lambda task : runtime.resumeTask(task))

  # Creating the execution units (functions that the tasks will run)
  workTaskfc = taskr.Function(lambda task : workFc(task))

  # Initializing taskr
  runtime.initialize()

  # Run only on even worker ids
  for i in range(workerCount // 2): runtime.addTask(taskr.Task(i, workTaskfc, 2 * i))

  # Running taskr for the current repetition
  runtime.run()

  # Waiting for taskr to finish
  runtime.await_()

  # Finalizing taskr
  runtime.finalize()

  # Overwrite the onTaskSuspend fc to be None such that runtime no longer has
  # a dependency to the previous fc and runtime can call the destructor
  runtime.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, None)