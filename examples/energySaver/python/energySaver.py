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

def workFc(iterations):
  value = 2.0
  for i in range(iterations):
    for j in range(iterations):
      value = (value + i)**0.5
      value = value * value

def waitFc(taskr, secondsDelay):
  print("Starting long task...\n", flush=True)
  time.sleep(secondsDelay)
  print("Finished long task...\n", flush=True)

def energySaver(runtime, workTaskCount, secondsDelay, iterations):
  # Creating task work function
  workFunction = taskr.Function(lambda task : workFc(iterations))

  # Creating task wait function
  waitFunction = taskr.Function(lambda task : waitFc(runtime, secondsDelay))

  # Creating a single wait task that suspends all workers except for one
  waitTask1 = taskr.Task(0, waitFunction)

  # Building task graph. First a lot of pure work tasks. The wait task depends on these
  for i in range(workTaskCount):
    workTask = taskr.Task(i + 1, workFunction)
    waitTask1.addDependency(workTask)
    runtime.addTask(workTask)

  # Creating another wait task
  waitTask2 = taskr.Task(2 * workTaskCount + 1, waitFunction)

  # Then creating another batch of work tasks that depends on the wait task
  for i in range(workTaskCount):
    workTask = taskr.Task(workTaskCount + i + 1, workFunction)

    # This work task waits on the first wait task
    workTask.addDependency(waitTask1)

    # The second wait task depends on this work task
    waitTask2.addDependency(workTask)

    # Adding work task
    runtime.addTask(workTask)

  # Last set of work tasks
  for i in range(workTaskCount):
    workTask = taskr.Task(2 * workTaskCount + i + 2, workFunction)

    # This work task depends on the second wait task
    workTask.addDependency(waitTask2)

    # Adding work task
    runtime.addTask(workTask)

  # Adding work tasks
  runtime.addTask(waitTask1)
  runtime.addTask(waitTask2)

  # Initializing taskr
  runtime.initialize()

  # Running taskr
  print("Starting (open 'htop' in another console to see the workers going to sleep during the long task)...\n")
  runtime.run()
  runtime.wait()
  print("Finished.\n")

  # Finalizing taskr
  runtime.finalize()