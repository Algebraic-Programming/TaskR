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

import time
import numpy as np
import taskr

NTASKS = 2

N = 1000

def mmm(task):
  A = np.zeros((N,N))
  B = np.empty((N,N))
  C = np.empty((N,N))

  for i in range(N):
    for j in range(N):
      B[i, j] = i
      C[i, j] = j

  B += task.getLabel()+1
  C += task.getLabel()+1
  
  A = B @ C
  print(f"Hello, I am task {task.getLabel()}")


def mmmDriver(runtime):
  # Initializing taskr
  runtime.initialize()

  taskfc = taskr.Function(mmm)

  # Adding to tasks to taskr
  for i in range(NTASKS):
    runtime.addTask(taskr.Task(i, taskfc, i))

  # Running taskr for the current repetition
  t_start = time.time()
  runtime.run()

  # Waiting current repetition to end
  runtime.await_()
  t_duration = time.time() - t_start

  print(f"total time: {t_duration}s")

  # Finalizing taskr
  runtime.finalize()