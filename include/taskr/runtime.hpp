/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file runtime.hpp
 * @brief This file implements TaskR, an example tasking runtime class implemented with the HiCR tasking frontend
 * @author Sergio Martin
 * @date 8/8/2023
 */

#pragma once

#include <atomic>
#include <map>
#include <mutex>
#include <memory>
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/tasking.hpp>
#include <hicr/core/concurrent/queue.hpp>
#include <hicr/core/concurrent/hashMap.hpp>
#include <hicr/core/concurrent/hashSet.hpp>
#include "task.hpp"

/**
 * Required by the concurrent hash map implementation, the theoretical maximum number of entries in the active task queue
 */
#define __TASKR_DEFAULT_MAX_ACTIVE_TASKS 157810688

/**
 * Required by the concurrent hash map implementation, the theoretical maximum number of entries in the active worker queue
 */
#define __TASKR_DEFAULT_MAX_ACTIVE_WORKERS 65536

namespace taskr
{

/**
 * Implementation of a tasking runtime class implemented with the HiCR tasking frontend
 *
 * It holds the entire running state of the tasks and the dependency graph.
 */
class Runtime
{
  public:

  /**
   * Constructor of the TaskR Runtime.
   */
  Runtime(HiCR::L1::ComputeManager* computeManager) : _computeManager(computeManager)
  {
    // Creating internal objects
    _dispatcher           = std::make_unique<HiCR::tasking::Dispatcher>([this]() { return pullReadyTask(); });
    _waitingTaskQueue       = std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_ACTIVE_TASKS);
    _suspendedWorkerQueue = std::make_unique<HiCR::concurrent::Queue<HiCR::tasking::Worker>>(__TASKR_DEFAULT_MAX_ACTIVE_WORKERS);

    // Setting task callback functions
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskExecute, [this](HiCR::tasking::Task *task) {
      // Getting TaskR task pointer
      auto taskrTask = (taskr::Task *)task;

      // If defined, trigger user-defined event
      this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskExecute);
    });

    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [this](HiCR::tasking::Task *task) {
      // Getting TaskR task pointer
      auto taskrTask = (taskr::Task *)task;

      // Getting task label
      const auto taskLabel = taskrTask->getLabel();

      // Adding task to the finished object set
      _finishedObjects.insert(taskLabel);

      // If defined, trigger user-defined event
      this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskFinish);

      // Decreasing active task counter
      _activeTaskCount--;
    });

    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSuspend, [this](HiCR::tasking::Task *task) {
      // Getting TaskR task pointer
      auto taskrTask = (taskr::Task *)task;

      // If defined, trigger user-defined event
      this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSuspend);
    });

    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSync, [this](HiCR::tasking::Task *task) {
      // Getting TaskR task pointer
      auto taskrTask = (taskr::Task *)task;

      // If defined, trigger user-defined event
      this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSync);
    });
  }

  // Destructor 
  ~Runtime() = default;


  ///////////// Getting internal compute manager
  HiCR::L1::ComputeManager* getComputeManager() const { return _computeManager; }

  ///////////// Local tasking API

  /**
   * Adds a callback for a particular callback
   *
   * \param[in] event The callback event to assin the callback to
   * \param[in] fc The callback function to call when the event is triggered
   */
  __INLINE__ void setCallbackHandler(const HiCR::tasking::Task::callback_t event, HiCR::tasking::callbackFc_t<taskr::Task *> fc) { _taskrCallbackMap.setCallback(event, fc); }

  /**
   * This function adds a processing unit to be used by TaskR in the execution of tasks
   *
   * \param[in] pu The processing unit to add
   */
  __INLINE__ void addProcessingUnit(std::unique_ptr<HiCR::L0::ProcessingUnit> pu) { _processingUnits.push_back(std::move(pu)); }

  /**
   * Sets the maximum active worker count. If the current number of active workers exceeds this maximu, TaskR will put as many
   * workers to sleep as necessary to get the active count as close as this value as possible. If this maximum is later increased,
   * then any suspended workers will be awaken by active workers.
   *
   * \param[in] max The desired number of maximum workers. A non-positive value means that there is no limit.
   */
  __INLINE__ void setMaximumActiveWorkers(const size_t max)
  {
    // Storing new maximum active worker count
    _maximumActiveWorkers = max;
  }

  /**
   * Adds a task to the TaskR runtime for future execution. This can be called at any point, before or during the execution of TaskR.
   *
   * \param[in] task Task to add.
   */
  __INLINE__ void addTask(taskr::Task *task)
  {
    // Increasing active task counter
    _activeTaskCount++;

    // Making sure the task has its callback map correctly assigned
    task->setCallbackMap(&_hicrCallbackMap);

    // Add task to the ready queue
    _waitingTaskQueue->push(task);
  }

  /**
   * Re-activates (resumes) a task by adding it back to the waiting task queue
   *
   * \param[in] task Task to resume.
   */
  __INLINE__ void resumeTask(taskr::Task *task)
  {
    _waitingTaskQueue->push(task);
  }

  /**
   * This function represents the main loop of a worker that is looking for work to do.
   * It first checks whether the maximum number of worker is exceeded. If that's the case, it enters suspension and returns upon restart.
   * Otherwise, it finds a task in the waiting queue and checks its dependencies. If the task is ready to go, it runs it.
   * If no tasks are ready to go, it returns a nullptr, which encodes -No Task-.
   *
   * \return A pointer to a HiCR task to execute. nullptr if there are no pending tasks.
   */
  __INLINE__ taskr::Task *pullReadyTask()
  {
    // If all tasks finished, then terminate execution immediately
    if (_activeTaskCount == 0)
    {
      // Getting a pointer to the currently executing worker
      auto worker = HiCR::tasking::Worker::getCurrentWorker();

      // Terminating worker.
      worker->terminate();

      // Returning a nullptr function
      return nullptr;
    }

    // If maximum active workers is defined, then check if the threshold is exceeded
    checkMaximumActiveWorkerCount();

    // Poping next task from the lock-free queue
    auto task = _waitingTaskQueue->pop();

    // If no task was found (queue was empty), then return an empty task
    if (task == nullptr) return nullptr;

    // Checking for task's pending dependencies
    while(task->getDependencies().empty() == false)
    {
      // Checking whether the dependency is already finished
      const auto dependency = task->getDependencies().front();

      // If it is not finished:
      if (_finishedObjects.contains(dependency) == false) [[likely]]
       {
         // Push the task back into back of the queue
         _waitingTaskQueue->push(task);

         // Return nullptr, signaling no task was found
         return nullptr;
       }

      // Otherwise, remove it out of the dependency queue
      task->getDependencies().pop();
    }

    // The task's dependencies may be satisfied, but now we got to check whether it has any pending operations
    while(task->getPendingOperations().empty() == false)
    {
      // Checking whether the operation has finished
      const auto pendingOperation = task->getPendingOperations().front();

      // Running pending operation checker
      const auto result = pendingOperation();

      // If not satisfied:
      if (result == false)
      {
         // Push the task back into back of the queue
        _waitingTaskQueue->push(task);

        // Return nullptr, signaling no task was found
        return nullptr;
      } 

      // Otherwise, remove it out of the dependency queue
      task->getPendingOperations().pop();
    }

    // Returning ready task
    return task;
  }


  /**
   * Initailizes the TaskR runtime
   * Creates a set of HiCR workers, based on the provided computeManager, and subscribes them to a dispatcher queue.
   */
  __INLINE__ void initialize()
  {
    // Initializing HiCR tasking
    HiCR::tasking::initialize();

    // Creating one worker per processung unit in the list
    for (auto &pu : _processingUnits)
    {
      // Creating new worker
      auto worker = new HiCR::tasking::Worker(_computeManager);

      // Assigning resource to the thread
      worker->addProcessingUnit(std::move(pu));

      // Assigning worker to the common dispatcher
      worker->subscribe(_dispatcher.get());

      // Finally adding worker to the worker set
      _workers.push_back(worker);
    }
  }

   /**
   * Finalizes the TaskR runtime
   * Releases all workers and frees up their memory
   */
  __INLINE__ void finalize()
  {
    // Clearing created objects
    for (auto &w : _workers) delete w;
    _workers.clear();

    // Finalizing HiCR tasking
    HiCR::tasking::finalize();
  }

  /**
   * Starts the execution of the TaskR runtime.
   * It starts the workers and suspends the current context until they're back (all tasks have finished).
   */
  __INLINE__ void run()
  {
    // Initializing active worker count
    _activeWorkerCount = _workers.size();

    // Initializing workers
    for (auto &w : _workers) w->initialize();

    // Starting workers
    for (auto &w : _workers) w->start();

    // Waiting for workers to finish computing
    for (auto &w : _workers) w->await();
  }

  private:

  /**
   * This function implements the auto-sleep mechanism that limits the number of active workers based on a user configuration
   * It will put any calling worker to sleep if the number of active workers exceed the maximum.
   * If the number of active workers is smaller than the maximum, it will try to 'wake up' other suspended workers, if any,
   * until the maximum is reached again.
   */
  __INLINE__ void checkMaximumActiveWorkerCount()
  {
    // Getting a pointer to the currently executing worker
    auto worker = HiCR::tasking::Worker::getCurrentWorker();

    // Try to get the active worker queue lock, otherwise keep going
    if (_activeWorkerQueueLock.try_lock())
    {
      // If the number of workers exceeds that of maximum active workers,
      // suspend current worker
      if (_maximumActiveWorkers > 0 && _activeWorkerCount > (ssize_t)_maximumActiveWorkers)
      {
        // Adding worker to the queue
        _suspendedWorkerQueue->push(worker);

        // Reducing active worker count
        _activeWorkerCount--;

        // Releasing lock, this is necessary here because the worker is going
        // into sleep
        _activeWorkerQueueLock.unlock();

        // Suspending worker
        worker->suspend();

        // Returning now, because we've already released the lock and shouldn't
        // do anything else without it
        return;
      }

      // If the new maximum is higher than the number of active workers, we need
      // to re-awaken some of them
      while ((_maximumActiveWorkers == 0 || (ssize_t)_maximumActiveWorkers > _activeWorkerCount) && _suspendedWorkerQueue->wasEmpty() == false)
      {
        // Getting the worker from the queue of suspended workers
        auto w = _suspendedWorkerQueue->pop();

        // Do the following if a worker was obtained
        if (w != nullptr)
        {
          // Increase the active worker count
          _activeWorkerCount++;

          // Resuming worker
          if (w != nullptr) w->resume();
        }
      }

      // Releasing lock
      _activeWorkerQueueLock.unlock();
    }
  }

  /**
   * Pointer to the compute manager to use
   */
  HiCR::L1::ComputeManager* const _computeManager;

  /**
   * Type definition for the task's callback map
   */
  typedef HiCR::tasking::CallbackMap<taskr::Task *, HiCR::tasking::Task::callback_t> taskrCallbackMap_t;

  /**
   *  HiCR callback map shared by all tasks
   */
  HiCR::tasking::Task::taskCallbackMap_t _hicrCallbackMap;

  /**
   *  TaskR-specific callmap, customizable by the user
   */
  taskrCallbackMap_t _taskrCallbackMap;

  /**
   * Single dispatcher that distributes pending tasks to idle workers as they become idle
   */
  std::unique_ptr<HiCR::tasking::Dispatcher> _dispatcher;

  /**
   * Set of workers assigned to execute tasks
   */
  std::vector<HiCR::tasking::Worker *> _workers;

  /**
   * Mutex for the active worker queue, required for the max active workers mechanism
   */
  std::mutex _activeWorkerQueueLock;

  /**
   * Counter for the current number of active tasks. Execution finishes when this counter reaches zero
   */
  std::atomic<size_t> _activeTaskCount = 0;

  /**
   * User-defined maximum active worker count. Required for the max active workers mechanism
   * A zero value indicates that there is no limitation to the maximum active worker count.
   */
  size_t _maximumActiveWorkers = 0;

  /**
   * Keeps track of the currently active worker count. Required for the max active workers mechanism
   */
  ssize_t _activeWorkerCount;

  /**
   * Lock-free queue for waiting tasks.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _waitingTaskQueue;

  /**
   * Lock-free queue storing workers that remain in suspension. Required for the max active workers mechanism
   */
  std::unique_ptr<HiCR::concurrent::Queue<HiCR::tasking::Worker>> _suspendedWorkerQueue;

  /**
   * The processing units assigned to taskr to run workers from
   */
  std::vector<std::unique_ptr<HiCR::L0::ProcessingUnit>> _processingUnits;

  /**
   * This parallel set stores the id of all finished objects
   */
  HiCR::concurrent::HashSet<HiCR::tasking::uniqueId_t> _finishedObjects;
}; // class Runtime

} // namespace taskr
