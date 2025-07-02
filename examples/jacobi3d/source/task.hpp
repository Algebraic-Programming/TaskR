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

#pragma once

#include <taskr/taskr.hpp>

// Jacobi Task, wraps a taskr task with metadata about the task's coordinates
class Task final : public taskr::Task
{
  public:

  Task(std::string taskType, ssize_t i, ssize_t j, ssize_t k, ssize_t iteration, taskr::Function *fc)
    : taskr::Task(encodeTaskName(taskType, i, j, k, iteration), fc),
      i(i),
      j(j),
      k(k),
      iteration(iteration)
  {}

  ~Task() = default;

  // X coordinate (index i);
  const ssize_t i;

  // Y coordinate (index j);
  const ssize_t j;

  // Z coordinate (index k);
  const ssize_t k;

  // Iteration this task corresponds to
  const ssize_t iteration;

  // Function to encode a task name into a unique identifier
  static inline size_t encodeTaskName(const std::string &taskName, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint64_t iter)
  {
    char buffer[512];
    sprintf(buffer, "%s_%lu_%lu_%lu_%lu", taskName.c_str(), lx, ly, lz, iter);
    const std::hash<std::string> hasher;
    const auto                   hashResult = hasher(buffer);

    // find if this hash already exists in the hashmap if not: add it
    size_t tasklabel;
    auto   it = taskid_hashmap.find(hashResult);

    if (it == taskid_hashmap.end())
    {
      tasklabel = taskid_hashmap.size();

      taskid_hashmap[hashResult] = tasklabel;
    }
    else { tasklabel = it->second; }

    return tasklabel;
  }
};