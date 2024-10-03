/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file task.hpp
 * @brief This file implements the TaskR task class
 * @author Sergio Martin
 * @date 02/10/2024
 */

#pragma once

#include "task.hpp"
#include "function.hpp"

namespace taskr
{

  /**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   *
   * @param[in] label The unique label to assign to this task
   * @param[in] executionUnit Specifies the function/kernel to execute.
   * @param[in] workerAffinity The worker affinity to set from the start. Default -1 indicates no affinity.
   */
  __INLINE__ Task::Task(const label_t label, Function* fc, const workerId_t workerAffinity)
    : taskr::Object(label),
      HiCR::tasking::Task(fc->getExecutionUnit(), nullptr),
      _workerAffinity(workerAffinity)
  {}

/**
 * Returns the task/worker affinity
 * 
 * @return The worker affinity currently set for this task
 */
__INLINE__ workerId_t Task::getWorkerAffinity() const { return _workerAffinity; }

/**
 * Sets the task's worker affinity. 
 * 
 * @param[in] workerAffinity The worker affinity to set
 */
__INLINE__ void Task::setWorkerAffinity(const workerId_t workerAffinity) { _workerAffinity = workerAffinity; }


} // namespace taskr
