/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file task.hpp
 * @brief This file implements the TaskR task class
 * @author Sergio Martin
 * @date 29/7/2024
 */

#pragma once

#include <hicr/backends/pthreads/L1/computeManager.hpp>
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
    : _executionUnit(
        HiCR::backend::pthreads::L1::ComputeManager::createExecutionUnit([fc](void *task) { fc(static_cast<taskr::Task *>(static_cast<HiCR::tasking::Task *>(task))); }))
  {}

  /**
   * Returns the internal execution unit
   * 
   * @return The function's internal execution unit
   */
  __INLINE__ std::shared_ptr<HiCR::L0::ExecutionUnit> getExecutionUnit() const { return _executionUnit; }

  private:

  /**
   * Represents the internal HiCR-based execution unit to be replicated to run this function
   */
  const std::shared_ptr<HiCR::L0::ExecutionUnit> _executionUnit;

}; // class Task

} // namespace taskr
