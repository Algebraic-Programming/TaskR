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

#include <chrono>
#include <hicr/frontends/tasking/common.hpp>
#include "common.hpp"
#include "task.hpp"

namespace taskr
{

/**
 * This class defines a TaskR
 *
 * This is a function that is executed with a given frequency and is useful to detect asynchronous events, such as incoming messages
 * 
 * Services are picked up by any free task / service worker and executed. While a service is executed, no other worker can execute it in parallel.
 * 
 * Services do not represent the critical path of an application. Their presence not preclude taskR from finished when there are no tasks left to execute.
 */
class Service
{
  public:

  /**
   * The type of a service function
   */
  typedef std::function<void()> serviceFc_t;

  Service()          = delete;
  virtual ~Service() = default;

  /**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   *
   * @param[in] fc Specifies the TaskR-formatted function to use
   * @param[in] interval The minimum interval in ms between two executions of the service. Specify 0 for no minimum interval.
   */
  Service(serviceFc_t fc, const size_t interval) : _fc(fc), _interval(interval) { }

  /** 
   * This function indicates whether the minimum interval has passed between the last execution.
   * 
   * @return If the interval has passed,  then it is considered active (true) and should be executed. Otherwise, it should return false.
   * 
  */
  __INLINE__ bool isActive() const
  {
    const auto timeElapsedSinceLastExecution = (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _lastExecutionTime).count();
    if (timeElapsedSinceLastExecution >= _interval) return true;
    return false;
  }

  /** 
   * This function runs the underlying service function
   * 
   * It updates the last time the service ran, after executing it (so that actual run time does not count towards the waiting interval)
   * 
  */
  __INLINE__ void run()
  {
    _fc();
    _lastExecutionTime = std::chrono::high_resolution_clock::now();
  }
  

  private:

  /**
   * Abstract definition of a time point
   */
  typedef std::chrono::high_resolution_clock::time_point timePoint_t;

  /**
   * Function for the service to execute
   */
  const serviceFc_t _fc;

  /**
   * The interval between consecutive executions
   */
  const size_t _interval;

  /**
   * Time point storing the last time this service was executed
   */
  timePoint_t _lastExecutionTime{};
}; // class Service

} // namespace taskr
