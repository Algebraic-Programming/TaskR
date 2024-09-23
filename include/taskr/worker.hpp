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

#include <chrono>
#include <hicr/core/concurrent/queue.hpp>
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/worker.hpp>
#include "common.hpp"
#include "task.hpp"

namespace taskr
{

/**
 * This class defines the basic processing unit managed by TaskR.
 */
class Worker : public HiCR::tasking::Worker
{
  public:

  Worker()  = delete;
  ~Worker() = default;

  /**
   * Constructor for the TaskR worker class.
   * 
   * \param[in] computeManager A backend's compute manager, meant to initialize and run the task's execution states.
   * \param[in] pullFunction A callback for the worker to get a new task to execute
   */
  __INLINE__ Worker(HiCR::L1::ComputeManager *computeManager, HiCR::tasking::pullFunction_t pullFunction)
    : HiCR::tasking::Worker(computeManager, pullFunction),
    _waitingTaskQueue(std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_WORKER_ACTIVE_TASKS))
  { }

  auto getWaitingTaskQueue() const { return _waitingTaskQueue.get(); }

  void updateActivityTime() { _lastActivityTime = std::chrono::high_resolution_clock::now(); }
  size_t getTimeSinceLastActivityMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(_lastActivityTime - std::chrono::high_resolution_clock::now()).count(); };

  private:

  /**
   * Activity time registers the last time the worker had something useful to do.
   * This is used to put the thread to sleep after a time of inactivity
   */
  std::chrono::high_resolution_clock::time_point _lastActivityTime;

  /**
  * Worker-specific lock-free queue for waiting tasks.
  */
  const std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _waitingTaskQueue;

}; // class Worker

} // namespace taskr
