"""
    Copyright 2025 Huawei Technologies Co., Ltd.
 
  Licensed under the Apache License, Version 2.0 (the "License")
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
import time
import taskr
import workTask

def main():
    # Getting work task count
    workTaskCount = 4
    iterations    = 100
    if len(sys.argv) > 1: workTaskCount = int(sys.argv[1])
    if len(sys.argv) > 2: iterations = int(sys.argv[2])

    


    # Getting the core subset from the argument list (could be from a file too)
    coreSubset = {0, 1, 2, 3}
    if len(sys.argv) > 3:
        coreSubset = {}
        for i in range(3, len(sys.argv)):
            coreSubset.add(int(sys.argv[i]))

    # Sanity check
    if not len(coreSubset):
        sys.stderr.write("Launch error: no compute resources provided")
        sys.exit(1)
    
    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr("threading", coreSubset)

    # Get the runtime
    runtime = t.get_runtime()

    # Creating task function
    taskFunction = taskr.Function(lambda task : workTask.work(iterations))

    # Adding multiple compute tasks
    print(f"Running {workTaskCount} work tasks with {len(coreSubset)} processing units...")
    for i in range(workTaskCount):
        task = taskr.Task(i, taskFunction)
        runtime.addTask(task)

    # Initializing taskR
    runtime.initialize()

    # Running taskr only on the core subset
    t0 = time.time()
    runtime.run()
    runtime.await_()
    tf = time.time()

    dt = tf - t0
    print(f"Finished in {dt:.3} seconds.")

    # Finalizing taskR
    runtime.finalize()

if __name__ == "__main__":
    main()