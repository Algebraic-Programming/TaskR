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

import sys
import time
import taskr

def conditionVariableWaitForCondition(runtime):
  # Contention value
  value = 0

  # Mutex for the condition variable
  mutex = taskr.Mutex()

  # Task-aware conditional variable
  cv = taskr.ConditionVariable()

  # Time for timeout checking (Microseconds)
  timeoutTimeUs = 100 * 1000

  # Forever time to wait
  forever = 1000 * 1000 * 1000

  def fc(task):
    # Using lock to update the value
    print("Thread 1: I go first and set value to 1")
    mutex.lock(task)
    value = 1
    mutex.unlock(task)

    # Notifiying the other thread
    print("Thread 1: Now I notify anybody waiting")
    while value != 2:
      cv.notifyOne(task)
      task.suspend()

    # Waiting for the other thread's update now
    print("Thread 1: I wait (forever) for the value to turn 2")
    mutex.lock(task)
    wasNotified = cv.waitFor(task, mutex, lambda : value == 2, forever)
    mutex.unlock(task)

    if not wasNotified:
      sys.stderr.write("Error: I have returned due to a timeout!")
      sys.exit(1)

    print("Thread 1: The condition (value == 2) is satisfied now")

    # Now waiting for a condition that won't be met, although we'll get notifications
    print("Thread 1: I wait (with timeout) for the value to turn 3 (won't happen)")
    mutex.lock(task)
    startTime = time.time_ns()*1e-3
    wasNotified = cv.waitFor(task, mutex, lambda : value == 3, timeoutTimeUs)
    currentTime = time.time_ns()*1e-3
    elapsedTime = currentTime - startTime
    mutex.unlock(task)

    if wasNotified:
      sys.stderr.write("Error: I have returned do to a notification!")
      sys.exit(1)
    
    if elapsedTime < timeoutTimeUs:
      sys.stderr.write("Error: I have returned earlier than expected!")
      sys.exit(1)
    
    print(f"Thread 1: I've exited by timeout (as expected in {elapsedTime}us >= {timeoutTimeUs}us)")

    # Updating value to 3 now, to release the other thread
    mutex.lock(task)
    value = 3
    mutex.unlock(task)

  # Creating task functions
  thread1Fc = taskr.Function(fc)

  def fc(task):
    # Waiting for the other thread to set the first value
    print("Thread 2: First, I'll wait for the value to become 1")
    mutex.lock(task)
    wasNotified = cv.waitFor(task, mutex, lambda :  value == 1, forever)
    mutex.unlock(task)
    if not wasNotified:
      sys.stderr.write("Error: I have returned do to a timeout!")
      sys.exit(1)
    
    print("Thread 2: The condition (value == 1) is satisfied now")

    # Now updating the value ourselves
    print("Thread 2: Now I update the value to 2")
    mutex.lock(task)
    value = 2
    mutex.unlock(task)

    # Notifying the other thread
    print("Thread 2: Notifying constantly until the value is 3")
    while value != 3:
      cv.notifyOne(task)
      task.suspend()
    

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
  expectedValue = 3
  print(f"Value {value} / Expected {expectedValue}")

  assert value == expectedValue
