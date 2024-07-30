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

  typedef HiCR::tasking::uniqueId_t label_t;

  Object()  = delete;
  ~Object() = default;

  Object(const label_t label) : _label(label) { }

  label_t getLabel() const { return _label; }

  virtual bool isReady() const = 0;

  //// Input dependency management

  __INLINE__ ssize_t getInputDependencyCounter() const { return _inputDependencyCounter.load(); }
  __INLINE__ ssize_t increaseInputDependencyCounter() { return (_inputDependencyCounter.fetch_add(1)) + 1; }
  __INLINE__ ssize_t decreaseInputDependencyCounter() { return (_inputDependencyCounter.fetch_sub(1)) - 1; }

  //// Output dependency management

  __INLINE__ void addOutputDependency(const HiCR::tasking::uniqueId_t dependency) { _outputDependencies.push_back(dependency); }
  const std::vector<HiCR::tasking::uniqueId_t>& getOutputDependencies() const { return _outputDependencies; }

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
