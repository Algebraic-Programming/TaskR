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

def conditionVariableWaitCondition(runtime):
  # Contention value
  value = 0

  # Mutex for the condition variable
  mutex = taskr.Mutex()

  # Task-aware conditional variable
  cv = taskr.ConditionVariable()

  def fc(task):
    nonlocal  value
    # Using lock to update the value
    print("Thread 1: I go first and set value to 1")
    mutex.lock(task)
    value += 1
    mutex.unlock(task)

    # Notifiying the other thread
    print("Thread 1: Now I notify anybody waiting")
    while value != 1:
      cv.notifyOne(task)
      task.suspend()

    # Waiting for the other thread's update now
    print("Thread 1: I wait for the value to turn 2")
    mutex.lock(task)
    cv.wait(task, mutex, lambda: value == 2)
    mutex.unlock(task)
    print("Thread 1: The condition (value == 2) is satisfied now")

  # Creating task functions
  thread1Fc = taskr.Function(fc)

  def fc(task):
    nonlocal  value
    # Waiting for the other thread to set the first value
    print("Thread 2: First, I'll wait for the value to become 1")
    mutex.lock(task)
    cv.wait(task, mutex,  lambda: value == 1)
    mutex.unlock(task)
    print("Thread 2: The condition (value == 1) is satisfied now")

    # Now updating the value ourselves
    print("Thread 2: Now I update the value to 2")
    mutex.lock(task)
    value += 1
    mutex.unlock(task)

    # Notifying the other thread
    print("Thread 2: Notifying anybody interested")
    cv.notifyOne(task)

  thread2Fc = taskr.Function(fc)

  task1 = taskr.Task(0, thread1Fc)
  task2 = taskr.Task(1, thread2Fc)

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

  # Value should be equal to concurrent task count
  expectedValue = 2
  print(f"Value {value} / Expected {expectedValue}")
  
  assert value == expectedValue
