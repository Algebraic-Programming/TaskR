/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file conditionVariable.hpp
 * @brief Provides a class definition for a task-aware condition variable object
 * @author S. M. Martin
 * @date 25/3/2024
 */

#pragma once

#include <chrono>
#include <queue>
#include <hicr/core/concurrent/queue.hpp>
#include "mutex.hpp"
#include "task.hpp"

namespace taskr
{

/**
 * Implementation of a task-aware Condition Variable in TaskR.
*/
class ConditionVariable
{
  public:

  ConditionVariable()  = default;
  ~ConditionVariable() = default;

  /**
   * Suspends the tasks unconditionally, and resumes after notification
   * 
   * \note The suspension of the task will not block the running thread.
   * 
   * \param[in] currentTask A pointer to the currently running task
   * \param[in] conditionMutex The mutual exclusion mechanism to use to prevent two tasks from evaluating the condition predicate simultaneously
  */
  void wait(taskr::Task *currentTask, taskr::Mutex &conditionMutex)
  {
    // Insert oneself in the waiting task list
    _mutex.lock(currentTask);
    bool notificationFlag = false;
    _waitingTasks.push(&notificationFlag);
    _mutex.unlock(currentTask);

    // Adding pending operation
    currentTask->addPendingOperation([&]() { return notificationFlag; });

    // Releasing cv lock
    conditionMutex.unlock(currentTask);

    // Suspending task now
    currentTask->suspend();

    // Retaking lock
    conditionMutex.lock(currentTask);
  }

  /**
   * (1) Checks whether the given condition predicate evaluates to true.
   *  - If it does, then returns immediately.
   *  - If it does not, adds the task to the notification list and suspends it.
   *    - When resumed, the task will repeat step (1)
   * 
   * \note The suspension of the task will not block the running thread.
   * \param[in] currentTask A pointer to the currently running task
   * \param[in] conditionMutex The mutual exclusion mechanism to use to prevent two tasks from evaluating the condition predicate simultaneously
   * \param[in] conditionPredicate The function that returns a boolean true if the condition is satisfied; false, if not.
  */
  void wait(taskr::Task *currentTask, taskr::Mutex &conditionMutex, const std::function<bool(void)> &conditionPredicate)
  {
    // Asserting I am the owner of the condition mutx
    if (conditionMutex.ownsLock(currentTask) == false) HICR_THROW_LOGIC("Condition variable: trying to use a mutex that doesn't belong to this task");

    // Checking on the condition
    bool keepWaiting = conditionPredicate() == false;

    // If the condition is not satisfied, suspend until we're notified and the condition is satisfied
    while (keepWaiting == true)
    {
      // Insert oneself in the waiting task list
      _mutex.lock(currentTask);
      bool notificationFlag = false;
      _waitingTasks.push(&notificationFlag);
      _mutex.unlock(currentTask);

      // Adding pending operation
      currentTask->addPendingOperation([&]() { return notificationFlag; });

      // Releasing cv lock
      conditionMutex.unlock(currentTask);

      // Suspending task now
      currentTask->suspend();

      // After being notified, check on the condition again
      conditionMutex.lock(currentTask);
      keepWaiting = conditionPredicate() == false;
    }
  }

  /**
   * Checks whether the given condition predicate evaluates to true.
   *  - If it does, then returns immediately.
   *  - If it does not, adds the task to the notification list and suspends it.
   *    - When resumed, the task will check total wait time
   *       - If wait time is smaller than timeout, repeat step (1)
   *       - If wat time exceeds timeout, return immediately
   * 
   * \note The suspension of the task will not block the running thread.
   * \param[in] currentTask A pointer to the currently running task
   * \param[in] conditionMutex The mutual exclusion mechanism to use to prevent two tasks from evaluating the condition predicate simultaneously
   * \param[in] conditionPredicate The function that returns a boolean true if the condition is satisfied; false, if not.
   * \param[in] timeout The amount of microseconds provided as timeout 
   * 
   * \return True, if the task is returning before timeout; false, if otherwise.
  */
  bool waitFor(taskr::Task *currentTask, taskr::Mutex &conditionMutex, const std::function<bool(void)> &conditionPredicate, size_t timeout)
  {
    // Asserting I am the owner of the condition mutx
    if (conditionMutex.ownsLock(currentTask) == false) HICR_THROW_LOGIC("Condition variable: trying to use a mutex that doesn't belong to this task");

    // Checking on the condition
    bool predicateSatisfied = conditionPredicate();

    // If predicate is satisfied, return immediately
    if (predicateSatisfied == true) return true;

    // Flag indicating the task has been notified
    bool isTimeout        = false;
    bool notificationFlag = false;

    // Taking current time
    auto startTime = std::chrono::high_resolution_clock::now();

    // While the condition predicate hasn't been met
    while (predicateSatisfied == false && isTimeout == false)
    {
      // Insert oneself in the waiting task list
      _mutex.lock(currentTask);
      notificationFlag = false;
      _waitingTasks.push(&notificationFlag);
      _mutex.unlock(currentTask);

      // Adding pending operation
      currentTask->addPendingOperation([&]() {
        // Checking for timeout
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
        if (elapsedTime > std::chrono::duration<size_t, std::micro>(timeout))
        {
          // Setting notification flag to prevent a notification to arrive after this timeout
          isTimeout = true;

          // Returning true (task is ready to continue)
          return true;
        }

        // Checking notification
        if (notificationFlag == true) return true;

        // Otherwise not ready to continue yet
        return false;
      });

      // Releasing cv lock
      conditionMutex.unlock(currentTask);

      // Suspending task now
      currentTask->suspend();

      // Retaking cv lock
      conditionMutex.lock(currentTask);

      // After being notified, check on the condition again. Only if not by timeout
      if (isTimeout == false) predicateSatisfied = conditionPredicate();
    }

    // Return true if the exit was due to satisfied condition predicate
    return predicateSatisfied;
  }

  /**
   * Suspends the tasks unconditionally, and resumes after notification
   *
   * \note The suspension of the task will not block the running thread.
   * 
   * \param[in] currentTask A pointer to the currently running task
   * \param[in] conditionMutex The mutual exclusion mechanism to use to prevent two tasks from evaluating the condition predicate simultaneously
   * \param[in] timeout The amount of microseconds provided as timeout 
   * \return True, if the task is returning before timeout; false, if otherwise.
  */
  bool waitFor(taskr::Task *currentTask, taskr::Mutex &conditionMutex, size_t timeout)
  {
    // Asserting I am the owner of the condition mutx
    if (conditionMutex.ownsLock(currentTask) == false) HICR_THROW_LOGIC("Condition variable: trying to use a mutex that doesn't belong to this task");

    // Flag indicating the task has been notified
    bool returnsDueToNotification = false;
    bool notificationFlag         = false;

    // Insert oneself in the asynchronous waiting task list
    _mutex.lock(currentTask);
    _waitingTasks.push(&notificationFlag);
    _mutex.unlock(currentTask);

    // Taking current time
    auto startTime = std::chrono::high_resolution_clock::now();

    // Adding pending operation
    currentTask->addPendingOperation([&]() {
      // Checking notification
      if (notificationFlag == true)
      {
        // Specify we are returning due to a notification
        returnsDueToNotification = true;

        // Returning true (task is ready to continue)
        return true;
      }

      // Checking for timeout
      auto currentTime = std::chrono::high_resolution_clock::now();
      auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
      if (elapsedTime > std::chrono::duration<size_t, std::micro>(timeout))
      {
        // Setting notification flag to prevent a notification to arrive after this timeout
        notificationFlag = true;

        // Returning true (task is ready to continue)
        return true;
      }

      // Otherwise not ready to continue yet
      return false;
    });

    // Releasing cv lock
    conditionMutex.unlock(currentTask);

    // Suspending task now
    currentTask->suspend();

    // Retaking cv lock
    conditionMutex.lock(currentTask);

    // Return reason for continuing
    return returnsDueToNotification;
  }

  /**
   * Enables (notifies) one of the waiting tasks to check for the condition again.
   * 
   * \param[in] currentTask A pointer to the currently running task
  */
  void notifyOne(taskr::Task *currentTask)
  {
    _mutex.lock(currentTask);

    // If there is a task waiting to be notified, do that now and take it out of the queue
    if (_waitingTasks.empty() == false)
    {
      auto notificationFlag = _waitingTasks.front();
      _waitingTasks.pop();
      *notificationFlag = true;
    };

    // Releasing queue lock
    _mutex.unlock(currentTask);
  }

  /**
   * Enables (notifies) all of the waiting tasks to check for the condition again.
   * 
   * \param[in] currentTask A pointer to the currently running task
  */
  void notifyAll(taskr::Task *currentTask)
  {
    _mutex.lock(currentTask);

    // If there are tasks waiting to be notified, do that now and take them out of the queue
    while (_waitingTasks.empty() == false)
    {
      auto *notificationFlag = _waitingTasks.front();
      *notificationFlag      = true;
      _waitingTasks.pop();
    }

    _mutex.unlock(currentTask);
  }

  /**
   * Gets the number of tasks already waiting for a notification
   * 
   * @return The number of tasks already waiting for a notification
  */
  [[nodiscard]] size_t getWaitingTaskCount() const { return _waitingTasks.size(); }

  private:

  using notificationFlag_t = bool;

  /**
   * Internal mutex for accessing the waiting task set
  */
  taskr::Mutex _mutex;

  /**
   * A set of waiting tasks for synchronous notification. No ordering is enforced here.
  */
  std::queue<bool *> _waitingTasks;
};

} // namespace taskr
