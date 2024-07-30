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
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/tasking.hpp>
#include <hicr/core/concurrent/queue.hpp>
#include <hicr/core/concurrent/hashMap.hpp>
#include "task.hpp"

#define __TASKR_DEFAULT_MAX_ACTIVE_TASKS 65536
#define __TASKR_DEFAULT_MAX_ACTIVE_WORKERS 4096

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
  Runtime(const size_t maxTasks = __TASKR_DEFAULT_MAX_ACTIVE_TASKS, const size_t maxWorkers = __TASKR_DEFAULT_MAX_ACTIVE_WORKERS)
    : _maxActiveTasks(maxTasks),
      _maxWorkers(maxWorkers)
  {
    _dispatcher           = new HiCR::tasking::Dispatcher([this]() { return pullReadyTask(); });
    _readyTaskQueue     = new HiCR::concurrent::Queue<taskr::Task>(maxTasks);
    _suspendedWorkerQueue = new HiCR::concurrent::Queue<HiCR::tasking::Worker>(maxWorkers);

    // Setting task callback functions
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskExecute, [this](HiCR::tasking::Task* task)
    {
     // Getting TaskR task pointer
     auto taskrTask = (taskr::Task*) task;

     // If defined, trigger user-defined event
     this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskExecute);
    });

    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [this](HiCR::tasking::Task* task)
    {
     // Getting TaskR task pointer
     auto taskrTask = (taskr::Task*) task;

     // Getting task label
     const auto taskLabel = taskrTask->getLabel();

     // Notifying dependent tasks of the completion of this task
     for (const auto dependentId : taskrTask->getOutputDependencies())
     {
       // Getting the corresponding task from its id
       auto dependentTask = _taskMap[dependentId];

       // Decrease their input dependency counter
       dependentTask->decreaseInputDependencyCounter();

       // If the task is ready to go, add it back now
       if (dependentTask->isReady()) resumeTask(dependentTask);
     }

     // If defined, trigger user-defined event
     this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskFinish);

     // Removing task from the task map
     _taskMap.erase(taskLabel);

     // Free-up memory now the task is finished 
     delete taskrTask;

     // If this is the last task, we can finish now
     if (_taskMap.size() == 0) finalize();
    });

    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSuspend, [this](HiCR::tasking::Task*  task)
    {
     // Getting TaskR task pointer
     auto taskrTask = (taskr::Task*) task;

     // If defined, trigger user-defined event
     this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSuspend);
    });

    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSync, [this](HiCR::tasking::Task* task)
    {
     // Getting TaskR task pointer
     auto taskrTask = (taskr::Task*) task;

     // If defined, trigger user-defined event
     this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSync);
    });
  }

  // Destructor (frees previous allocations)
  ~Runtime()
  {
    delete _dispatcher;
    delete _readyTaskQueue;
    delete _suspendedWorkerQueue;
  }

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
    // Making sure the task has its callback map correctly assigned
    task->setCallbackMap(&_hicrCallbackMap);

    // Getting task label
    const auto taskLabel = task->getLabel();

    // Adding task to the task map
    _taskMap[taskLabel] = task;
  }

  /** 
  * Resumes the execution of a task
  *
  * \param[in] task Task to resume
  */
  __INLINE__ void resumeTask(taskr::Task* task)
  {
     // Add task to the ready queue
    _readyTaskQueue->push(task);
  }

   /**
   * Adds a new dependency between two tasks
   * 
   * @param[in] dependentTask The task to add a new dependency on
   * @param[in] dependedTask the depended task of the event upon which the task depends
   * 
   */
    __INLINE__ void addDependency(taskr::Task* const dependentTask, taskr::Task* const dependedTask)
    {
      // First, increase the task's input dependency counter
      dependentTask->increaseInputDependencyCounter();

      // Then append the task as output dependency to the event
      dependedTask->addOutputDependency(dependentTask->getLabel());
    }

  /**
   * This function represents the main loop of a worker that is looking for work to do.
   * It first checks whether the maximum number of worker is exceeded. If that's the case, it enters suspension and returns upon restart.
   * Otherwise, it finds a task in the waiting queue and checks its dependencies. If the task is ready to go, it runs it.
   * If no tasks are ready to go, it returns a NULL, which encodes -No Task-.
   *
   * \return A pointer to a HiCR task to execute. NULL if there are no pending tasks.
   */
  __INLINE__ taskr::Task *pullReadyTask()
  {
    // If all tasks finished, then terminate execution immediately
    if (_continueRunning == false)
    {
      // Getting a pointer to the currently executing worker
      auto worker = HiCR::tasking::Worker::getCurrentWorker();

      // Terminating worker.
      worker->terminate();

      // Returning a NULL function
      return NULL;
    }

    // If maximum active workers is defined, then check if the threshold is exceeded
    checkMaximumActiveWorkerCount();

    // Poping next task from the lock-free queue
    auto task = _readyTaskQueue->pop();

    // If no task was found (queue was empty), then return an empty task
    if (task == NULL) return NULL;

    // Returning ready task
    return task;
  }

  /**
   * Starts the execution of the TaskR runtime.
   * Creates a set of HiCR workers, based on the provided computeManager, and subscribes them to a dispatcher queue.
   * After creating the workers, it starts them and suspends the current context until they're back (all tasks have finished).
   *
   * \param[in] computeManager The compute manager to use to coordinate the execution of processing units and tasks
   */
  __INLINE__ void run(HiCR::L1::ComputeManager *computeManager)
  {
    // Initializing HiCR tasking
    HiCR::tasking::initialize();

    // Adding all tasks that have no dependencies at the time of running
    for (const auto& entry : _taskMap)
    {
      // Getting task's pointer
      auto task = entry.second;
      
      // If the task is ready, add it to the ready queue
      if (task->isReady()) _readyTaskQueue->push(task);
    }

    // Set runtime as running
    _continueRunning = true;

    // Creating one worker per processung unit in the list
    for (auto &pu : _processingUnits)
    {
      // Creating new worker
      auto worker = new HiCR::tasking::Worker(computeManager);

      // Assigning resource to the thread
      worker->addProcessingUnit(std::move(pu));

      // Assigning worker to the common dispatcher
      worker->subscribe(_dispatcher);

      // Finally adding worker to the worker set
      _workers.push_back(worker);

      // Initializing worker
      worker->initialize();
    }
    
    // Initializing active worker count
    _activeWorkerCount = _workers.size();

    // Starting workers
    for (auto &w : _workers) w->start();

    // Waiting for workers to finish
    for (auto &w : _workers) w->await();

    // Clearing created objects
    for (auto &w : _workers) delete w;
    _workers.clear();

    // Finalizing HiCR tasking
    HiCR::tasking::finalize();
  }
 
  /*
  * Signals the runtime to finalize as soon as possible
  */
  __INLINE__ void finalize()
  {
     _continueRunning = false;
  }

  private:

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
  HiCR::tasking::Dispatcher *_dispatcher;

  /**
   * Set of workers assigned to execute tasks
   */
  std::vector<HiCR::tasking::Worker *> _workers;

  /**
   * indicates whether the runtime must continue running or finish
   */
  bool _continueRunning = false;

  /**
   * Mutex for the active worker queue, required for the max active workers mechanism
   */
  std::mutex _activeWorkerQueueLock;

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
   * Lock-free queue for ready tasks.
   */
  HiCR::concurrent::Queue<taskr::Task> *_readyTaskQueue;

  /**
   * Lock-free queue storing workers that remain in suspension. Required for the max active workers mechanism
   */
  HiCR::concurrent::Queue<HiCR::tasking::Worker> *_suspendedWorkerQueue;

  /**
   * The processing units assigned to taskr to run workers from
   */
  std::vector<std::unique_ptr<HiCR::L0::ProcessingUnit>> _processingUnits;

  /**
   * Task map to relate a task label to its pointer
   */
  HiCR::concurrent::HashMap<taskr::Task::label_t, taskr::Task*> _taskMap;

  /**
   * Determines the maximum amount of active tasks (required by the lock-free queue)
  */
  const size_t _maxActiveTasks;

  /**
   * Determines the maximum amount of workers (required by the lock-free queue)
  */
  const size_t _maxWorkers;

  /**
   * Custom callback for task termination. Useful for freeing up task memory during execution
   */
  bool _customOnTaskFinishCallbackDefined = false;
  HiCR::tasking::callbackFc_t<taskr::Task> _customOnTaskFinishCallbackFunction;

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
        if (w != NULL)
        {
          // Increase the active worker count
          _activeWorkerCount++;

          // Resuming worker
          if (w != NULL) w->resume();
        }
      }

      // Releasing lock
      _activeWorkerQueueLock.unlock();
    }
  }

}; // class Runtime

} // namespace taskr
