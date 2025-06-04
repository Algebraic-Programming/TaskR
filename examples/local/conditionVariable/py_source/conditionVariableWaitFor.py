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

import sys
import time
import taskr

def conditionVariableWaitFor(runtime):
  # Contention value
  value = 0

  # Mutex for the condition variable
  mutex = taskr.Mutex()

  # Task-aware conditional variable
  cv = taskr.ConditionVariable()

  # Time for timeout checking (Microseconds)
  timeoutTimeUs = 100 * 1000

  # Forever time to wait (for notification-only waits)
  forever = 1000 * 1000 * 1000

  def fc(task):
    # Waiting for the other task's notification
    print("Thread 1: I wait for a notification (Waiting for an hour)")

    mutex.lock(task)
    wasNotified = cv.waitFor(task, mutex, forever)
    mutex.unlock(task)
    if not was_notified:
      sys.stderr.write("Error: I have returned due to a timeout!")
      sys.exit(1)

    print("Thread 1: I have been notified (as expected)")

    value = 1

    # Waiting for a timeout
    print(f"Thread 1: I wait for a timeout (Waiting for {timeoutTimeUs}ms) ")

    mutex.lock(task)
    startTime = time.time_ns()*1e-3
    wasNotified = cv.waitFor(task, mutex, timeoutTimeUs)
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

  # Creating task functions
  waitFc = taskr.Function(fc)

  def fc(task):
    # Notifying the other task
    print("Thread 2: Notifying anybody interested (only once)")
    while value != 1:
      cv.notifyOne(task)
      task.suspend()

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
