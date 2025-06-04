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

import taskr

def conditionVariableWait(runtime):
  # Contention value
  value = 0

  # Mutex for the condition variable
  mutex = taskr.Mutex()

  # Task-aware conditional variable
  cv = taskr.ConditionVariable()

  def fc(task):
    nonlocal  value
    # Waiting for the other task's notification
    print("Thread 1: I wait for a notification")
    mutex.lock(task)
    print("before cv.wait(task, mutex)", flush=True)
    cv.wait(task, mutex)
    print("after cv.wait(task, mutex)", flush=True)
    mutex.unlock(task)
    value = 1
    print("Thread 1: I have been notified")

  # Creating task functions
  waitFc = taskr.Function(fc)

  def fc(task):
    nonlocal  value
    # Notifying the other task
    print("Thread 2: Notifying anybody interested")
    while value != 1:
      cv.notifyOne(task)
      print("cv.notifyOne(task)", flush=True)
      task.suspend()
      print("task.suspend()", flush=True)

  notifyFc = taskr.Function(fc)

  task1 = taskr.Task(0, waitFc)
  task2 = taskr.Task(1, notifyFc)

  runtime.addTask(task1)
  runtime.addTask(task2)

  # Initializing taskr
  runtime.initialize()

  # Running taskr
  runtime.run()

  # Waiting for task to finish
  runtime.await_()

  # Finalizing taskr
  runtime.finalize()
