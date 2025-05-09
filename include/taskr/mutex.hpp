/*
 *   Copyright 2025 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file mutex.hpp
 * @brief Provides a class definition for a task-aware mutex
 * @author S. M. Martin
 * @date 25/3/2024
 */

#pragma once

#include <mutex>
#include <queue>
#include <hicr/core/exceptions.hpp>
#include "task.hpp"

namespace taskr
{

/**
 * Implementation of a TaskR task-aware mutual exclusion mechanism
*/
class Mutex
{
  public:

  Mutex()  = default;
  ~Mutex() = default;

  /**
   * Checks whether a given task owns the lock
   * 
   * \param[in] task The task to check ownership for
   * @return True, if the task owns the lock; false, otherwise.
  */
  __INLINE__ bool ownsLock(taskr::Task *task) { return _ownerTask == task; }

  /**
   * Tries to obtain the lock and returns immediately if it fails.
   *
   * \param[in] task The task acquiring the lock
   * @return True, if it succeeded in obtaining the lock; false, otherwise.
  */
  __INLINE__ bool trylock(taskr::Task *task)
  {
    _internalMutex.lock();
    bool success = lockNotBlockingImpl(task);
    _internalMutex.unlock();
    return success;
  }

  /**
   * Obtains the mutual exclusion lock.
   * 
   * In case the lock is currently owned by a different task, it will suspend the currently running task and
   * not allow it to run again until the lock is gained.
   * 
   * \param[in] task The task acquiring the lock
  */
  __INLINE__ void lock(taskr::Task *task) { lockBlockingImpl(task); }

  /**
   * Releases a lock currently owned by the currently running task.
   * 
   * \param[in] task The task releasing the lock
   * \note This function will produce an exception if trying to unlock a mutex not owned by the calling task. 
  */
  __INLINE__ void unlock(taskr::Task *task)
  {
    if (ownsLock(task) == false) HICR_THROW_LOGIC("Trying to unlock a mutex that doesn't belong to this task");

    _internalMutex.lock();

    _ownerTask = nullptr;
    if (_queue.empty() == false)
    {
      _ownerTask = _queue.front();
      _queue.pop();
    }

    _internalMutex.unlock();
  }

  private:

  /**
   * Internal implementation of the blocking lock obtaining mechanism
   * 
   * \param[in] task The task acquiring the lock
  */
  __INLINE__ void lockBlockingImpl(taskr::Task *task)
  {
    _internalMutex.lock();

    bool isLockFree = lockNotBlockingImpl(task);

    // If not successful, then insert task in the pending queue and suspend it
    if (isLockFree == false)
    {
      // Adding a new pending operation for the task to prevent from re-executing task until the lock is obtained
      task->addPendingOperation([&]() { return _ownerTask == task; });

      // Adding itself to the queue
      _queue.push(task);

      // Releasing lock
      _internalMutex.unlock();

      // Suspending task now
      task->suspend();

      // After suspension, you can return (mutex is already unlocked)
      return;
    }

    _internalMutex.unlock();
  }

  /**
   * Internal implementation of the non-blocking lock obtaining mechanism
   * 
   * \param[in] task The desired value to assign to the lock value. If the expected value is observed, then this value is assigned atomically to the locl value.
   * \return True, if succeeded in acquiring the lock; false, otherwise.
  */
  __INLINE__ bool lockNotBlockingImpl(taskr::Task *task)
  {
    bool isLockFree = _ownerTask == nullptr;
    if (isLockFree) _ownerTask = task;
    return isLockFree;
  }

  /**
   * Internal mutex for protecting the internal state and the task queue
  */
  std::mutex _internalMutex;

  /**
   * Internal state
  */
  taskr::Task *_ownerTask = nullptr;

  /**
   * Pending task queue
  */
  std::queue<taskr::Task *> _queue;
};

} // namespace taskr