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
   * @return The value before the operation was performed
   */
  __INLINE__ size_t addDependency() { return _dependencyCount.fetch_add(1); }

    /**
   * Adds one output dependency on the current object
   * 
   * This dependency represents an object (local or remote) that cannot be ready until this object finishes
   * 
   * @param[in] dependency The label of the object that depends on this object
   * @return The value before the operation was performed
   */
  __INLINE__ size_t removeDependency() { return _dependencyCount.fetch_sub(1); }

  /**
    * Gets a reference to the task's pending dependencies
    * 
    * @return A reference to the queue containing the task's pending dependencies
    */
  __INLINE__ size_t getDependencyCount() { return _dependencyCount.load(); }
  
  protected:

  /**
   * Unique identifier for the task
   */
  const label_t _label;

  /**
   * This holds all the objects this object depends on
   */
  std::atomic<size_t> _dependencyCount{0};

}; // class Task

} // namespace taskr
