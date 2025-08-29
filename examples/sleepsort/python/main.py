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
import time
import numpy as np

np.random.seed(42)

def main():
    t = taskr.create("threading")

    t.initialize()

    time_buffer_scale = 0.001
    n = 100
    shuffled_array = np.arange(n)
    sorted_array = []

    np.random.shuffle(shuffled_array)

    def fc(task):
        nonlocal sorted_array
        nonlocal shuffled_array

        id = task.getTaskId()

        value = shuffled_array[id]

        time.sleep(time_buffer_scale*value)

        sorted_array.append(value)

    taskfc = taskr.Function(fc)

    for i in range(n):
        t.addTask(taskr.Task(i, taskfc))

    t_start = time.time()
    t.run()
    t.wait()
    assert np.all(sorted_array[:-1] <= sorted_array[1:]), "Array is not sorted!"
    print(f"Total time [s]: {time.time() - t_start:0.5}")

    t.finalize()


if __name__ == "__main__":
    main()