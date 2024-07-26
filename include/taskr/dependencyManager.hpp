/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file deployer.hpp
 * @brief This file implements the HiCR task class
 * @author Sergio Martin
 * @date 8/8/2023
 */

#pragma once

#include <hicr/core/concurrent/hashMap.hpp>
#include "common.hpp"

namespace HiCR
{

namespace tasking
{

/**
 * This class defines a generic implementation of an event dependency manager
 *
 * A dependency represents a dependent/depended relationship between an event and a callback. 
 *  * Dependents are represented by a counter that starts non-zero and is reduced by one every time one of its dependents is completed
 *    * When the dependent's counter reaches zero, it executes an associated callback
 *  * Dependend events are set as completed by manual calls to its respective function
 *  
 */
class DependencyManager final
{
  public:

  typedef HiCR::tasking::uniqueId_t eventId_t;
  typedef HiCR::tasking::callbackFc_t<eventId_t> eventCallbackFc_t; 
  

  DependencyManager(eventCallbackFc_t eventTriggerCallback)
  : _eventTriggerCallback(eventTriggerCallback)
  { }

  DependencyManager() = delete;
  ~DependencyManager() = default;

  __INLINE__ void addDependency(const eventId_t dependentId, const eventId_t dependedId)
  {
    // Increasing dependency counter of the dependent
    _inputDependencyCounterMap[dependentId]++;

    // Adding output dependency in the depended task
    _outputDependencyMap[dependedId].push_back(dependentId);
  }

  __INLINE__ void triggerEvent(const eventId_t dependedId)
  {
     // For all dependents of the given depended id:
     for (const auto dependentId : _outputDependencyMap[dependedId])
     {
       // Decrease their input dependency counter
       _inputDependencyCounterMap[dependentId]--;

       // If the counter has reached zero:
       if (_inputDependencyCounterMap[dependentId] == 0)
       {
          // Remove it from the map (prevents uncontrolled memory use growth)
          _inputDependencyCounterMap.erase(dependentId);

          // And trigger the callback with the dependent id
          _eventTriggerCallback(dependentId);
       }
       
       // Removing output dependency from the map
       _outputDependencyMap.erase(dependedId);
     }
  }

  private:

  HiCR::concurrent::HashMap<eventId_t, size_t> _inputDependencyCounterMap;
  HiCR::concurrent::HashMap<eventId_t, std::vector<eventId_t>> _outputDependencyMap;
  const eventCallbackFc_t _eventTriggerCallback;

}; // class DependencyManager

} // namespace tasking

} // namespace HiCR
