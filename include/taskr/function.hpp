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

/**
 * @file task.hpp
 * @brief This file implements the TaskR task class
 * @author Sergio Martin
 * @date 29/7/2024
 */

#pragma once

#include <hicr/backends/pthreads/computeManager.hpp>
#include "common.hpp"
#include "task.hpp"

namespace taskr
{

/**
 * The shape of a function accepted by TaskR
 */
typedef std::function<void(taskr::Task *)> function_t;

/**
 * This class defines the basic execution unit managed by TaskR.
 *
 * It represents a function to execute with a pointer to the task executing it as argument
 */
class Function
{
  public:

  Function()  = delete;
  ~Function() = default;

  /**
   * Constructor for the TaskR funciton class. It requires a user-defined function to execute
   *
   * @param[in] fc Specifies the function to execute.
   */
  __INLINE__ Function(const function_t fc)
    : _executionUnit(HiCR::backend::pthreads::ComputeManager::createExecutionUnit([fc](void *task) { fc(static_cast<taskr::Task *>(static_cast<HiCR::tasking::Task *>(task))); }))
  {}

  /**
   * Returns the internal execution unit
   * 
   * @return The function's internal execution unit
   */
  __INLINE__ std::shared_ptr<HiCR::ExecutionUnit> getExecutionUnit() const { return _executionUnit; }

  private:

  /**
   * Represents the internal HiCR-based execution unit to be replicated to run this function
   */
  const std::shared_ptr<HiCR::ExecutionUnit> _executionUnit;

}; // class Task

} // namespace taskr
