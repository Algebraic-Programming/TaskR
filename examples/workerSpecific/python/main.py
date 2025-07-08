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
import workerSpecific

def main():

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.create()

    num_workers = t.get_num_workers()

    # Running workerSpecific example
    workerSpecific.workerSpecific(t, num_workers)

if __name__ == "__main__":
    main()