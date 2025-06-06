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

def simple(runtime):
  # Initializing taskr
  runtime.initialize()

  fc = lambda task : print(f"Hello, I am task {task.getLabel()}")

  # def fc(task):
  #   print(f"Before suspend task {task.getLabel()}")
  #   task.suspend()
  #   print(f"After suspend task {task.getLabel()}")

  # Create the taskr Tasks
  taskfc = taskr.Function(fc)

  # Creating the execution units (functions that the tasks will run)
  for i in range(1):
    task = taskr.Task(i, taskfc)

    # Adding to taskr
    runtime.addTask(task)

  # Running taskr for the current repetition
  runtime.run()

  # Waiting current repetition to end
  runtime.await_()

  # Finalizing taskr
  runtime.finalize()