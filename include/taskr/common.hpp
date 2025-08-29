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
 * A unique identifier for an object (task)
 */
typedef HiCR::tasking::uniqueId_t taskId_t;

/**
 * Type for a locally-unique worker identifier
 */
typedef ssize_t workerId_t;

/**
 * The type of a service
 */
typedef std::function<void()> service_t;

} // namespace taskr
