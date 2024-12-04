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

#include <detectr.hpp>

#include "task.hpp"
#include "function.hpp"

namespace taskr
{

/**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   */
__INLINE__ Task::Task(Function *fc, const workerId_t workerAffinity)
  : HiCR::tasking::Task(fc->getExecutionUnit(), nullptr),
    _workerAffinity(workerAffinity)
{}

/**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   */
__INLINE__ Task::Task(const label_t label, Function *fc, const workerId_t workerAffinity)
  : HiCR::tasking::Task(fc->getExecutionUnit(), nullptr),
    _label(label),
    _workerAffinity(workerAffinity)
{
  // DetectR init task
  INSTRUMENTATION_TASK_EXEC(_label);
}

/**
 * Destructor
 */
__INLINE__ Task::~Task()
{
  // DetectR end task
  INSTRUMENTATION_TASK_END(_label);
}

} // namespace taskr
