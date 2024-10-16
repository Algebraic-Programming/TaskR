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
#include "taskImpl.hpp"

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
      _readyTaskQueue(std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_WORKER_ACTIVE_TASKS))
  {}

  /**
  * Accessor for the worker's internal ready task queue 
  * 
  * @return The worker's internal ready task queue
  */
  __INLINE__ auto getReadyTaskQueue() const { return _readyTaskQueue.get(); }

  /**
   * Indicates the worker has failed to retrieve a task
   * It only updates its internal timer the first time it fails after a success, to keep track
   * of how long it was since the last time it succeeded
   */
  __INLINE__ void setFailedToRetrieveTask()
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
  __INLINE__ void resetRetrieveTaskSuccessFlag() { _hasFailedToRetrieveTask = false; }

  /**
   * Retrieves the time passed since the last task retrieving success
   * 
   * @return The time passed since the last task retrieving success in milliseconds
   */
  __INLINE__ size_t getTimeSinceFailedToRetrievetaskMs() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _failedToRetrieveTaskTime).count();
  };

  /**
   * Indicates the worker has failed to retrieve a task last time it tried
   * 
   * @return true, if has failed; false, if succeeded
   */
  __INLINE__ bool getHasFailedToRetrieveTask() const { return _hasFailedToRetrieveTask; }

  /**
   * Ths function is called at set intervals to check whether the worker must resume or not
   * 
   * @return true, if the worker must resume; false, if it must remain suspended
   */
  __INLINE__ bool checkResumeConditions() override { return _checkResumeFunction(this); }

  /**
   * This function enables TaskR set a TaskR-specific check resume funciton
   * 
   * @param fc The function that checks whether the worker may continue executing after being suspended
   */
  __INLINE__ void setCheckResumeFunction(std::function<bool(taskr::Worker *)> fc) { _checkResumeFunction = fc; };

  private:

  /**
   * Function to check whether the worker can resume after being suspended
   */
  std::function<bool(taskr::Worker *)> _checkResumeFunction;

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
  * Worker-specific lock-free queue for ready tasks.
  */
  const std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _readyTaskQueue;

}; // class Worker

} // namespace taskr
