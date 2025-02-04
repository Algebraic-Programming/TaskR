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

#if defined(INSTRUMENTATION_TASKS) || defined(INSTRUMENTATION_THREADS)
  #include <tracr.hpp>
#endif

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
  // TraCR init task (Maybe could be done inside the runtime.hpp)
  INSTRUMENTATION_TASK_INIT();
}

} // namespace taskr
