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
from matmul import matmul_cpp_Driver, matmul_numpy_Driver

def main():
    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr(taskr.HiCRBackend.nosv, 2)

    # Get the runtime
    runtime = t.get_runtime()

    # Running matmul example
    matmul_cpp_Driver(runtime)

    # matmul_numpy_Driver(runtime)


if __name__ == "__main__":
    main()