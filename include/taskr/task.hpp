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

namespace TaskR
{

/**
 * This class defines the basic execution unit managed by TaskR.
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

  typedef HiCR::tasking::uniqueId_t label_t;

  Task()  = delete;
  ~Task() = default;

  /**
   * Constructor for the TaskR task class. It requires a user-defined function to execute
   * The task is considered finished when the function runs to completion.
   *
   * @param[in] executionUnit Specifies the function/kernel to execute.
   * @param[in] callbackMap Pointer to the callback map callbacks to be called by the task
   */
  __INLINE__ Task(const label_t label, std::shared_ptr<HiCR::L0::ExecutionUnit> executionUnit) : HiCR::tasking::Task(executionUnit, nullptr), 
    _label(label)
    { }

  label_t getLabel() const { return _label; }

  private: 
  
  const label_t _label;

}; // class Task

} // namespace TaskR
