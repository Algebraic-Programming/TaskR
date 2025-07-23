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
from main import ITERATIONS
JOB_ID = 1

def job2(runtime):
  # Creating a storage for all the tasks we will create in this example
  tasks = [None] * (3 * ITERATIONS)

  # Creating the execution units (functions that the tasks will run)
  taskAfc = taskr.Function(lambda task : print(f"Job 1 - Task A {task.getTaskId()}"))
  taskBfc = taskr.Function(lambda task : print(f"Job 1 - Task B {task.getTaskId()}"))
  taskCfc = taskr.Function(lambda task : print(f"Job 1 - Task C {task.getTaskId()}"))

  # Now creating tasks
  for i in range(ITERATIONS):
    taskId = i * 3 + 1
    tasks[taskId] = taskr.Task(3 * ITERATIONS * JOB_ID + taskId, taskBfc)
  
  for i in range(ITERATIONS):
    taskId = i * 3 + 0
    tasks[taskId] = taskr.Task(3 * ITERATIONS * JOB_ID + taskId, taskAfc)
  
  for i in range(ITERATIONS):
    taskId = i * 3 + 2
    tasks[taskId] = taskr.Task(3 * ITERATIONS * JOB_ID + taskId, taskCfc)

  # Now creating the dependency graph
  for i in range(ITERATIONS): tasks[i * 3 + 2].addDependency(tasks[i * 3 + 1])
  for i in range(ITERATIONS): tasks[i * 3 + 1].addDependency(tasks[i * 3 + 0])
  for i in range(1, ITERATIONS): tasks[i * 3 + 0].addDependency(tasks[i * 3 - 1])

  # Adding tasks to TaskR runtime
  for task in tasks: runtime.addTask(task)