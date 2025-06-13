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
import energySaver

def main():
    # Getting arguments, if provided
    workTaskCount = 3
    secondsDelay  = 1
    iterations    = 100
    if len(sys.argv) > 1: workTaskCount = int(sys.argv[1])
    if len(sys.argv) > 2: secondsDelay = int(sys.argv[2])
    if len(sys.argv) > 3: iterations = int(sys.argv[3])

    print(sys.argv, workTaskCount, secondsDelay, iterations)

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr("threading")

    # Get the runtime
    runtime = t.get_runtime()

    # Running simple example
    energySaver.energySaver(runtime, workTaskCount, secondsDelay, iterations)

if __name__ == "__main__":
    main()