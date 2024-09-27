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

#include <hicr/frontends/tasking/common.hpp>

/**
 * Required by the concurrent hash map implementation, the theoretical maximum number of entries in the common active task queue
 */
#define __TASKR_DEFAULT_MAX_COMMON_ACTIVE_TASKS 4194304

/**
 * Required by the concurrent hash map implementation, the theoretical maximum number of entries in the task-specific active task queue
 */
#define __TASKR_DEFAULT_MAX_WORKER_ACTIVE_TASKS 32768

/**
 * Required by the concurrent hash map implementation, the theoretical maximum number of entries in the active worker queue
 */
#define __TASKR_DEFAULT_MAX_ACTIVE_WORKERS 8192

/**
 * Required by the concurrent hash map implementation, the theoretical maximum number of entries in the services queue
 */
#define __TASKR_DEFAULT_MAX_SERVICES 256

namespace taskr
{

/**
 * A unique identifier (label) for an object
 */
typedef HiCR::tasking::uniqueId_t label_t;

/**
 * Type for a locally-unique worker identifier
 */
typedef ssize_t workerId_t;

/**
 * The type of a service
 */
typedef std::function<void()> service_t;

} // namespace taskr
