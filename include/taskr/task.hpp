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
#include <hicr/frontends/tasking/task.hpp>
#include "common.hpp"
#include "object.hpp"

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
class Task : public taskr::Object, public HiCR::tasking::Task
{
  public:

  Task()  = delete;
  ~Task() = default;

  /**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   *
   * @param[in] label The unique label to assign to this task
   * @param[in] fc Specifies the TaskR-formatted function to use
   * @param[in] workerAffinity The worker affinity to set from the start. Default -1 indicates no affinity.
   */
  Task(const label_t label, Function* fc, const workerId_t workerAffinity = -1);

  /**
   * Returns the task/worker affinity
   * 
   * @return The worker affinity currently set for this task
   */
  workerId_t getWorkerAffinity() const;

  /**
   * Sets the task's worker affinity. 
   * 
   * @param[in] workerAffinity The worker affinity to set
   */
  void setWorkerAffinity(const workerId_t workerAffinity);

  private:

  /**
   * Represents the affinity to a given worker, if specified. -1 if not specified.
   * 
   * The task can only be ran by the designated worker.
   */
  workerId_t _workerAffinity;

}; // class Task

} // namespace taskr
