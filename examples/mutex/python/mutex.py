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

_CONCURRENT_TASKS = 4
_ITERATIONS_ = 100

def mutex(runtime):

  # Contention value
  value = 0

  # Task-aware mutex
  m = taskr.Mutex()

  def fc(task):
    nonlocal value

    for i in range(_ITERATIONS_):
      m.lock(task)
      value += 1
      m.unlock(task)

  # Create the taskr Tasks
  taskfc = taskr.Function(fc)

  # Creating the execution units (functions that the tasks will run)
  for i in range(_CONCURRENT_TASKS):
    task = taskr.Task(i, taskfc)

    # Adding to taskr
    runtime.addTask(task)
  
  # Initializing taskr
  runtime.initialize()

  # Running taskr for the current repetition
  runtime.run()

  # Waiting current repetition to end
  runtime.wait()

  # Finalizing taskr
  runtime.finalize()

  # Value should be equal to concurrent task count
  print(f"Value {value} / Expected {_CONCURRENT_TASKS * _ITERATIONS_}")
  assert value == _CONCURRENT_TASKS * _ITERATIONS_