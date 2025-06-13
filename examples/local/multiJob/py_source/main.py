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
ITERATIONS = 100

import taskr
import job1
import job2

def main():

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr("threading")

    # Get the runtime
    runtime = t.get_runtime()

    # Running multiJob example
    job1.job1(runtime)
    job2.job2(runtime)

    # Initializing taskr
    runtime.initialize()

    # Running taskr for the current repetition
    runtime.run()

    # Waiting current repetition to end
    runtime.await_()

    # Finalizing taskr
    runtime.finalize()

if __name__ == "__main__":
    main()