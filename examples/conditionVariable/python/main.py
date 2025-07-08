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

import os

import taskr

from conditionVariableWait import conditionVariableWait
from conditionVariableWaitCondition import conditionVariableWaitCondition
from conditionVariableWaitFor import conditionVariableWaitFor
from conditionVariableWaitForCondition import conditionVariableWaitForCondition

def main():

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.create()

    t.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, lambda task : t.resumeTask(task))

    # Get the enviromnent variable for which function to call
    test_function_name = os.getenv('__TEST_FUNCTION_')

    # Get the function from the global namespace
    test_function = globals()[test_function_name]

    # Call the function
    test_function(t)

    # Overwrite the onTaskSuspend fc to be None such that runtime no longer has
    # a dependency to the previous fc and can call the destructor
    t.setTaskCallbackHandler(taskr.TaskCallback.onTaskSuspend, None)


if __name__ == "__main__":
    main()