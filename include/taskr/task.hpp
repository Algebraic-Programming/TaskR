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

namespace taskr
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

  __INLINE__ bool isReady() const
  {
    bool ready = true;

    // If the counter is above zero, then the task is still not ready
    if (_inputDependencyCounter > 0) ready = false;

    return ready;
  }

  //// Input dependency management

  __INLINE__ size_t getInputDependencyCounter() const { return _inputDependencyCounter.load(); }
  __INLINE__ size_t increaseInputDependencyCounter() { return _inputDependencyCounter.fetch_add(1) + 1; }
  __INLINE__ size_t decreaseInputDependencyCounter() { return _inputDependencyCounter.fetch_sub(1) - 1; }

  //// Output dependency management

  __INLINE__ void addOutputDependency(const HiCR::tasking::uniqueId_t dependency) { _outputDependencies.push_back(dependency); }
  const std::vector<HiCR::tasking::uniqueId_t>& getOutputDependencies() const { return _outputDependencies; }

  private: 
  
  /**
   * Unique identifier for the task
   */
  const label_t _label;

  /**
   * Atomic counter for the tasks' input dependencies
   */
  std::atomic<ssize_t> _inputDependencyCounter = 0;

  /**
   * This is a map that relates unique event ids to a set of its task dependents (i.e., output dependencies).
   */
  std::vector<HiCR::tasking::uniqueId_t> _outputDependencies;
  
}; // class Task

} // namespace taskr
