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
import cpp_matmul

NTASKS = 2

def matmul_cpp_Driver(runtime):
  # Initializing taskr
  runtime.initialize()

  taskfc = taskr.Function(cpp_matmul.cpp_matmul)

  # Adding to tasks to taskr
  for i in range(NTASKS):
    runtime.addTask(taskr.Task(i, taskfc))

  # Running taskr for the current repetition
  t_start = time.time()
  runtime.run()

  # Waiting current repetition to end
  runtime.await_()
  print(f"total time: {time.time() - t_start}")

  # Finalizing taskr
  runtime.finalize()



def matmul_numpy_Driver(runtime):
  # Initializing taskr
  runtime.initialize()

  def matmul_numpy(task):
    N = 1000
    A = np.empty((N,N))
    B = np.empty((N,N))
    C = np.empty((N,N))

    for i in range(N):
      for j in range(N):
        B[i, j] = 1.0/(i + 1)
        C[i, j] = 1.0/(j + 1)

    B += task.getLabel()+1
    C += task.getLabel()+1
    
    A = B @ C

  taskfc = taskr.Function(matmul_numpy)

  # Adding to tasks to taskr
  for i in range(NTASKS):
    runtime.addTask(taskr.Task(i, taskfc))

  # Running taskr for the current repetition
  t_start = time.time()
  runtime.run()

  # Waiting current repetition to end
  runtime.await_()
  print(f"total time: {time.time() - t_start}")

  # Finalizing taskr
  runtime.finalize()