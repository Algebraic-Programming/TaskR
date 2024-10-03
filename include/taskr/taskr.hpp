/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file taskr.hpp
 * @brief The main include file for the TaskR runtime system
 * @author Sergio Martin
 * @date 8/8/2023
 */

#pragma once

#include "task.hpp"
#include "taskImpl.hpp"
#include "function.hpp"
#include "runtime.hpp"

namespace taskr
{

/**
   * Returns the currently executing TaskR task
   *
   * \return A pointer to the currently executing TaskR task
   */
__INLINE__ taskr::Task *getCurrentTask() { return (taskr::Task *)HiCR::tasking::Task::getCurrentTask(); }

} // namespace taskr
