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

#include <list>
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/task.hpp>
#include "common.hpp"
#include "task.hpp"

namespace taskr
{

class Function;

/**
 * This class defines a basic scheduling unit managed by TaskR.
 *
 * It includes a function to execute, an internal state, and an callback map that triggers callbacks (if defined) whenever a state transition occurs.
 *
 * The function represents the entire lifetime of the task. That is, a task executes a single function, the one provided by the user, and will reach a terminated state after the function is fully executed.
 *
 * A task may be suspended before the function is fully executed. This is either by voluntary yielding, or by reaching an synchronous operation that prompts it to suspend. These two suspension reasons will result in different states.
 */
class Task : public HiCR::tasking::Task
{
  public:

  /**
  * The definition of a pending operation. It needs to return a boolean indicating whether the operation has ended.
  */
  typedef std::function<bool()> pendingOperation_t;

  Task()  = delete;
  ~Task() = default;

  /**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   *
   * @param[in] fc Specifies the TaskR-formatted function to use
   * @param[in] workerAffinity The worker affinity to set from the start. Default -1 indicates no affinity.
   */
  Task(Function *fc, const workerId_t workerAffinity = -1);

  /**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   *
   * @param[in] label The unique label to assign to this task
   * @param[in] fc Specifies the TaskR-formatted function to use
   * @param[in] workerAffinity The worker affinity to set from the start. Default -1 indicates no affinity.
   */
  Task(const label_t label, Function *fc, const workerId_t workerAffinity = -1);

  /**
   * Returns the task/worker affinity
   * 
   * @return The worker affinity currently set for this task
   */
  __INLINE__ workerId_t getWorkerAffinity() const;

  /**
   * Sets the task's worker affinity. 
   * 
   * @param[in] workerAffinity The worker affinity to set
   */
  __INLINE__ void setWorkerAffinity(const workerId_t workerAffinity);

  /**
   * Function to obtain the task's label
   * 
   * @return The task's label
   */
  __INLINE__ label_t getLabel() const { return _label; }

  /**
   * Function to set the task's label
   * 
   * @param[in] label The label to set
   */
  __INLINE__ void setLabel(const label_t label) { _label = label; }

  /**
   * Adds one pending operation on the current task
   *
   * @param[in] pendingOperation A function that checks whether the pending operation has completed or not
   */
  __INLINE__ void addPendingOperation(const pendingOperation_t pendingOperation) { _pendingOperations.push_back(pendingOperation); }

  /**
    * Gets a reference to the task's pending operations
    * 
    * @return A reference to the queue containing the task's pending operations
    */
  __INLINE__ std::list<pendingOperation_t> &getPendingOperations() { return _pendingOperations; }

  __INLINE__ size_t incrementDependencyCount() { return _dependencyCount.fetch_add(1) + 1; }
  __INLINE__ size_t decrementDependencyCount() { return _dependencyCount.fetch_sub(1) - 1; }
  __INLINE__ size_t getDependencyCount() { return _dependencyCount.load(); }

  private:

  /**
   * Unique identifier for the task
   */
  label_t _label;

  /**
   * Represents the affinity to a given worker, if specified. -1 if not specified.
   * 
   * The task can only be ran by the designated worker.
   */
  workerId_t _workerAffinity;

  /**
  * This holds all pending operations the task needs to wait on
  */
  std::list<pendingOperation_t> _pendingOperations;

  /**
   * This holds a counter for the tasks this task depends on
   */
  std::atomic<size_t> _dependencyCount{0};

}; // class Task

} // namespace taskr
