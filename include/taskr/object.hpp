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

#include <atomic>
#include <vector>
#include <cstddef>
#include <hicr/frontends/tasking/common.hpp>

namespace taskr
{

/**
 * This class defines an abstract object in TaskR, which contains a label and dependencies.
 */
class Object
{
  public:

  /**
   * A unique identifier (label) for an object
   */
  typedef HiCR::tasking::uniqueId_t label_t;

  Object()  = delete;
  ~Object() = default;

  /**
   * Constructs an object by assigning it a label (should be unique within the context of the runtime)
   * 
   * @param[in] label The unique label to assign to this object
   */
  Object(const label_t label)
    : _label(label)
  {}

  /**
   * Function to obtain the object's label
   * 
   * @return The object's label
   */
  label_t getLabel() const { return _label; }

  /**
   * A function to determine whether the current object (in whatever of its incarnations) is ready to be used
   * 
   * @return True, if the object is ready; false, otherwise.
   */
  virtual bool isReady() const = 0;

  //// Input dependency management

  /**
   * Gets the current value of the remaining input dependencies
   * 
   * @return The remaining input dependency count
   */
  __INLINE__ ssize_t getInputDependencyCounter() const { return _inputDependencyCounter.load(); }

  /**
   * Increases the input dependency counter by one, and returns the new value atomically
   * 
   * @return The new number of input dependencies
   */
  __INLINE__ ssize_t increaseInputDependencyCounter() { return _inputDependencyCounter.fetch_add(1) + 1; }

  /**
   * Decreases the input dependency counter by one, and returns the new value atomically
   * 
   * @return The new number of input dependencies
   */
  __INLINE__ ssize_t decreaseInputDependencyCounter() { return _inputDependencyCounter.fetch_sub(1) - 1; }

  //// Output dependency management

  /**
   * Adds one output dependency on the current object
   * 
   * This dependency represents an object (local or remote) that cannot be ready until this object finishes
   * 
   * @param[in] dependency The label of the object that depends on this object
   */
  __INLINE__ void addOutputDependency(const label_t dependency) { _outputDependencies.push_back(dependency); }

  /**
   * Returns this object's set of output dependencies 
   * 
   * @return This object's set of output dependencies
   */
  __INLINE__ const std::vector<HiCR::tasking::uniqueId_t> &getOutputDependencies() const { return _outputDependencies; }

  protected:

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
