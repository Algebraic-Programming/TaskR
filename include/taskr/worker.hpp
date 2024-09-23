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
  {}

  /**
  * Accessor for the worker's internal task queue 
  * 
  * @return The worker's internal waiting task queue
  */
  auto getWaitingTaskQueue() const { return _waitingTaskQueue.get(); }

  /**
   * Indicates the worker has failed to retrieve a task
   * It only updates its internal timer the first time it fails after a success, to keep track
   * of how long it was since the last time it succeeded
   */
  void setFailedToRetrieveTask()
  {
    // Only update if previously succeeded (this is such that we remember the first time we failed in the current fail streak)
    if (_hasFailedToRetrieveTask == false)
    {
      _hasFailedToRetrieveTask  = true;
      _failedToRetrieveTaskTime = std::chrono::high_resolution_clock::now();
    }
  }

  /**
   *  Indicates the worker has succeeded to retrieve a task
   */
  void setSucceededToRetrieveTask() { _hasFailedToRetrieveTask = false; }

  /**
   * Retrieves the time passed since the last task retrieving success
   * 
   * @return The time passed since the last task retrieving success in milliseconds
   */
  size_t getTimeSinceFailedToRetrievetaskMs() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(_failedToRetrieveTaskTime - std::chrono::high_resolution_clock::now()).count();
  };

  /**
   * Indicates the worker has failed to retrieve a task last time it tried
   * 
   * @return true, if has failed; false, if succeeded
   */
  bool getHasFailedToRetrieveTask() const { return _hasFailedToRetrieveTask; }

  private:

  /**
   * Remembers whether the worker failed to retrieve a task last time.
   * This is used to put the thread to sleep after a time of inactivity
   */
  bool _hasFailedToRetrieveTask = false;

  /**
   * Activity time registers the last time the worker failed to retrieve a task (because there were none available)
   */
  std::chrono::high_resolution_clock::time_point _failedToRetrieveTaskTime;

  /**
  * Worker-specific lock-free queue for waiting tasks.
  */
  const std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _waitingTaskQueue;

}; // class Worker

} // namespace taskr
