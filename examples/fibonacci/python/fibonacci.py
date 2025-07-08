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

from atomic import AtomicLong
import time
import taskr

# Globally assigned variables
_runtime = None
_taskCounter = AtomicLong(0)

# Fibonacci without memoization to stress the tasking runtime
def fibonacci(currentTask, x):
  if x == 0: return 0
  if x == 1: return 1

  global _taskCounter
  global _runtime

  result1 = 0
  result2 = 0

  # Creating task functions
  def Fc1(task):
    nonlocal result1
    result1 = fibonacci(task, x - 1)

  def Fc2(task):
    nonlocal result2
    result2 = fibonacci(task, x - 2)

  fibFc1  = taskr.Function(Fc1)
  fibFc2  = taskr.Function(Fc2)

  # Creating two new tasks
  subTask1 = taskr.Task(_taskCounter.value, fibFc1)
  _taskCounter += 1
  subTask2 = taskr.Task(_taskCounter.value, fibFc2)
  _taskCounter += 1

  # Adding dependencies with the newly created tasks
  currentTask.addDependency(subTask1)
  currentTask.addDependency(subTask2)

  # Adding new tasks to TaskR
  _runtime.addTask(subTask1)
  _runtime.addTask(subTask2)

  # Suspending current task
  currentTask.suspend()

  return result1 + result2

def fibonacciDriver(initialValue, runtime):
  # Setting global variables
  global _taskCounter
  global _runtime
  _runtime = runtime

  # Storage for result
  result = 0

  # Creating task functions
  def Fc(task):
    nonlocal result
    result = fibonacci(task, initialValue)

  initialFc = taskr.Function(Fc)

  # Now creating tasks and their dependency graph
  initialTask = taskr.Task(_taskCounter.value, initialFc)
  _taskCounter += 1

  runtime.addTask(initialTask)

  # Initializing taskR
  runtime.initialize()

  # Running taskr
  startTime = time.time()
  runtime.run()
  runtime.wait()
  endTime = time.time()

  computeTime = endTime - startTime

  print(f"Running Time: {computeTime:.5f}s")
  print(f"Total Tasks: {_taskCounter.value}")

  # Finalizing taskR
  runtime.finalize()

  # Dereferencing this global instance to let runtime call his Destructor
  _runtime = None

  # Returning fibonacci value
  return result
