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
import taskr
import manyParallel

def main():
    # Getting arguments, if provided
    taskCount   = 2
    branchCount = 100
    if len(sys.argv) > 1: taskCount   = int(sys.argv[1])
    if len(sys.argv) > 2: branchCount = int(sys.argv[2])

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr(taskr.HiCRBackend.threading)

    # Get the runtime
    runtime = t.get_runtime()

    # Running simple example
    manyParallel.manyParallel(runtime, taskCount, branchCount)

if __name__ == "__main__":
    main()