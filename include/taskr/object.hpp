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
#include <list>
#include "common.hpp"

namespace taskr
{

/**
 * This class defines an abstract object in TaskR, which contains a label and dependencies.
 */
class Object
{
  public:

  /**
   * The definition of a pending operation. It needs to return a boolean indicating whether the operation has ended.
   */
  typedef std::function<bool()> pendingOperation_t;

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
  __INLINE__ label_t getLabel() const { return _label; }

  /**
   * Adds one output dependency on the current object
   * 
   * This dependency represents an object (local or remote) that cannot be ready until this object finishes
   * 
   * @param[in] dependency The label of the object that depends on this object
   */
  __INLINE__ void addDependency(const label_t dependency) { _dependencies.push_back(dependency); }

  /**
    * Gets a reference to the task's pending dependencies
    * 
    * @return A reference to the queue containing the task's pending dependencies
    */
  __INLINE__ std::list<label_t> &getDependencies() { return _dependencies; }

  /**
   * Adds one pending operation on the current object
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

  protected:

  /**
   * Unique identifier for the task
   */
  const label_t _label;

  /**
   * This holds all the objects this object depends on
   */
  std::list<label_t> _dependencies;

  /**
   * This holds all pending operations the object needs to wait on
   */
  std::list<pendingOperation_t> _pendingOperations;

}; // class Task

} // namespace taskr
