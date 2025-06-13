/*
 *   Copyright 2025 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <taskr/taskr.hpp>
#include <pytaskr/pyruntime.hpp>

int main(int argc, char **argv)
{
  // Creating taskr instance
  taskr::PyRuntime pytaskr("nosv", 0);

  // Getting the runtime
  taskr::Runtime &runtime = pytaskr.get_runtime();

  // Printing runtime
  printf("I got the runtime with nOS-V backend: %p\n", &runtime);

  return 0;
}
