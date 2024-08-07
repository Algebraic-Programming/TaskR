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
#include <set>
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
   * Adds one output dependency on the current object
   * 
   * This dependency represents an object (local or remote) that cannot be ready until this object finishes
   * 
   * @param[in] dependency The label of the object that depends on this object
   */
  __INLINE__ void addDependency(const label_t dependency) { _dependencies.insert(dependency); }

  __INLINE__ std::set<HiCR::tasking::uniqueId_t>& getDependencies() { return _dependencies; }

  protected:

  /**
   * Unique identifier for the task
   */
  const label_t _label;

  /**
   * This holds all the objects this object depends on
   */
  std::set<HiCR::tasking::uniqueId_t> _dependencies;

}; // class Task

} // namespace taskr
