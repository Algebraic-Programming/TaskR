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
#include <memory>
#include <thread>
#include <nlohmann_json/json.hpp>
#include <nlohmann_json/parser.hpp>
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/tasking.hpp>
#include <hicr/core/concurrent/queue.hpp>
#include <hicr/core/concurrent/hashMap.hpp>
#include <hicr/core/concurrent/hashSet.hpp>
#include "task.hpp"
#include "worker.hpp"

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
   * 
   * @param[in] computeManager The compute manager used to run workers
   * @param[in] config Optional configuration parameters passed in JSON format
   */
  Runtime(HiCR::L1::ComputeManager *computeManager, nlohmann::json config = nlohmann::json())
    : _computeManager(computeManager)
  {
    // Creating internal objects
    _commonWaitingTaskQueue = std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_COMMON_ACTIVE_TASKS);
    _suspendedTaskWorkerQueue   = std::make_unique<HiCR::concurrent::Queue<taskr::Worker>>(__TASKR_DEFAULT_MAX_ACTIVE_WORKERS);
    _serviceQueue           = std::make_unique<HiCR::concurrent::Queue<taskr::service_t>>(__TASKR_DEFAULT_MAX_SERVICES); 

    // Setting task callback functions
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskExecute, [this](HiCR::tasking::Task *task) { this->onTaskExecuteCallback(task); });
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [this](HiCR::tasking::Task *task) { this->onTaskFinishCallback(task); });
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSuspend, [this](HiCR::tasking::Task *task) { this->onTaskSuspendCallback(task); });
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSync, [this](HiCR::tasking::Task *task) { this->onTaskSyncCallback(task); });

    // Assigning configuration defaults
    _taskWorkerInactivityTimeMs = 100; // 100 ms for a task worker to suspend if it didn't find any suitable tasks to execute
    _minimumActiveTaskWorkers = 1; // Guarantee that there is at least one active task worker
    _serviceWorkerCount = 0; // No service workers (Typical setting for HPC applications)
    _makeTaskWorkersRunServices = true; // Since no service workers are created by default, have task workers check on services

    // Parsing configuration
    if (config.contains("Task Worker Inactivity Time (Ms)")) _taskWorkerInactivityTimeMs   = hicr::json::getNumber<ssize_t>(config, "Task Worker Inactivity Time (Ms)");
    if (config.contains("Minimum Active Task Workers"))      _minimumActiveTaskWorkers = hicr::json::getNumber<size_t>(config,  "Minimum Active Task Workers");
    if (config.contains("Service Worker Count"))        _serviceWorkerCount       = hicr::json::getNumber<size_t>(config,  "Service Worker Count");
    if (config.contains("Make Task Workers Run Services")) _makeTaskWorkersRunServices = hicr::json::getBoolean(config, "Make Task Workers Run Services");
  }

  // Destructor
  ~Runtime() = default;

  /**
   * Function that returns the compute manager originally provided to Taskr 
   * 
   * @return A pointer to the compute manager
   */
  HiCR::L1::ComputeManager *getComputeManager() const { return _computeManager; }

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
    resumeTask(task);
  }

  /**
   * Re-activates (resumes) a task by adding it back to the waiting task queue
   *
   * \param[in] task Task to resume.
   */
  __INLINE__ void resumeTask(taskr::Task *task)
  {
    // Getting task's affinity
    const auto taskAffinity = task->getWorkerAffinity();

    // Sanity Check
    if (taskAffinity >= (ssize_t)_taskWorkers.size())
      HICR_THROW_LOGIC("Invalid task affinity specified: %ld, which is larger than the largest worker id: %ld\n", taskAffinity, _taskWorkers.size() - 1);

    // If no affinity set: 
    if (taskAffinity < 0) 
    {
      // Push it into the common task queue
      _commonWaitingTaskQueue->push(task);

      // Wake up a task worker, to make sure there is somebody to execute it
      wakeUpOneTaskWorker();
    }

    // If the affinity was set:
    else
    {
      // Put it in the corresponding task worker's queue
      _taskWorkers[taskAffinity]->getWaitingTaskQueue()->push(task);

      // Wake worker up (regardless if it's suspended or not)
      _taskWorkers[taskAffinity]->resume();
    }
  }

  /**
   * Initailizes the TaskR runtime
   * Creates a set of HiCR workers, based on the provided computeManager, and subscribes them to the pull function.
   */
  __INLINE__ void initialize()
  {
    // Verify taskr is not currently initialized
    if (_isInitialized == true) HICR_THROW_LOGIC("Trying to initialize TaskR, but it is currently initialized");

    // Checking if we have at least one processing unit
    if (_processingUnits.empty()) HICR_THROW_LOGIC("Trying to initialize TaskR with no processing units assigned to it");

    // Initializing HiCR tasking
    HiCR::tasking::initialize();

    // Creating one worker per processung unit in the list
    for (size_t workerId = 0; workerId < _processingUnits.size(); workerId++)
    {
      // Creating new worker
      auto taskWorker = new taskr::Worker(_computeManager, [this, workerId]() -> taskr::Task * {

        // Getting the worker's pointer
        const auto worker = _taskWorkers[workerId];

        // If all tasks finished, then terminate execution immediately
        if (_activeTaskCount == 0)
        {
          // Waking up all other possibly asleep workers
          while (_activeTaskWorkerCount != _taskWorkers.size())
          {
            auto resumableWorker = _suspendedTaskWorkerQueue->pop();
            if (resumableWorker != nullptr) resumableWorker->resume();
          }

          // Getting a pointer to the currently executing worker
          auto worker = taskr::Worker::getCurrentWorker();

          // Terminating worker.
          worker->terminate();

          // Returning a nullptr function
          return nullptr;
        }

        // Getting next task to execute
        auto task = getNextTask(workerId);

        // If no found was found and this is the first failure since the last success
        if (task == nullptr)
        {
          // Set the worker's fail time, if not already set
          worker->setFailedToRetrieveTask();

          // Check for inactivity time (to put the worker to sleep)
          checkWorkerSuspension(worker);
        }

        // If a task was found
        if (task != nullptr)
        {
          // Setting worker as succeeded to retrieve task (resets the inactivity timer)
          worker->setSucceededToRetrieveTask();

          // If we've found a ready task, there might be other ready too. So we need to wake a worker up
          wakeUpOneTaskWorker();
        }

        // Returning task pointer regardless if found or not
        return task;
      });

      // Assigning resource to the thread
      taskWorker->addProcessingUnit(std::move(_processingUnits[workerId]));

      // Finally adding worker to the worker set
      _taskWorkers.push_back(taskWorker);
    }

    // Setting taskr as uninitialized
    _isInitialized = true;
  }

  /**
   * Finalizes the TaskR runtime
   * Releases all workers and frees up their memory
   */
  __INLINE__ void finalize()
  {
    // Verify taskr is currently initialized
    if (_isInitialized == false) HICR_THROW_LOGIC("Trying to initialize TaskR, but it is currently not initialized");

    // Setting taskr as uninitialized
    _isInitialized = false;

    // Clearing created objects
    for (auto &w : _taskWorkers) delete w;
    _taskWorkers.clear();

    // Finalizing HiCR tasking
    HiCR::tasking::finalize();
  }

  /**
   * Starts the execution of the TaskR runtime.
   * It starts the workers and suspends the current context until they're back (all tasks have finished).
   */
  __INLINE__ void run()
  {
    // Verify taskr was correctly initialized
    if (_isInitialized == false) HICR_THROW_LOGIC("Trying to run TaskR, but it was not initialized");

    // Setting the number of active workers
    _activeTaskWorkerCount = _taskWorkers.size();

    // Initializing workers
    for (auto &w : _taskWorkers) w->initialize();

    // Starting workers
    for (auto &w : _taskWorkers) w->start();

    // Waiting for workers to finish computing
    for (auto &w : _taskWorkers) w->await();
  }

  private:

  __INLINE__ void wakeUpOneTaskWorker()
  {
    // If all task workers are currently running, do not bother
    auto resumableWorker = _suspendedTaskWorkerQueue->pop();
    if (resumableWorker != nullptr) resumableWorker->resume();
  }

  /**
   * Function to check whether the running thread needs to suspend
   */
  __INLINE__ void checkWorkerSuspension(taskr::Worker *worker)
  {
    // Check for inactivity time (to put the worker to sleep)
    if (_taskWorkerInactivityTimeMs >= 0)                   // If this setting is, negative then no suspension is used
      if (worker->getHasFailedToRetrieveTask() == true) // If the worker has failed to retrieve a task last time
        if (worker->getTimeSinceFailedToRetrievetaskMs() > (size_t)_taskWorkerInactivityTimeMs)
        {
          // Reducing the number of active threads
          size_t actualActiveWorkers = _activeTaskWorkerCount.fetch_sub(1);

          // If we are already at the minimum, do not suspend. Otherwise, go ahead
          if (actualActiveWorkers > _minimumActiveTaskWorkers)
          {
            // Creating additional thread to suspend the current one atomically
            auto suspenderThread = std::thread([&]() {
              // Suspending worker
              worker->suspend();

              // Adding worker to suspended worker queue
              _suspendedTaskWorkerQueue->push(worker);
            });

            // Waiting until suspender thread finishes
            suspenderThread.join();
          }

          // Re-setting success flags to prevent immediate re-suspension
          worker->setSucceededToRetrieveTask();

          // Re-adding myself as active worker
          _activeTaskWorkerCount++;
        }
  }

  /**
   * This function represents the main loop of a worker that is looking for work to do.
   * It first checks whether the maximum number of worker is exceeded. If that's the case, it enters suspension and returns upon restart.
   * Otherwise, it finds a task in the waiting queue and checks its dependencies. If the task is ready to go, it runs it.
   * If no tasks are ready to go, it returns a nullptr, which encodes -No Task-.
   *
   * \param[in] workerId The identifier of the worker running this function. Used to pull from its own waiting task queue
   * \return A pointer to a HiCR task to execute. nullptr if there are no pending tasks.
   */
  __INLINE__ taskr::Task *getNextTask(const workerId_t workerId)
  {
    // Getting the worker's pointer
    const auto worker = _taskWorkers[workerId];

    // Trying to grab next task from my own task queue
    auto task = worker->getWaitingTaskQueue()->pop();

    // If nothing found there, poping next task from the common queue
    if (task == nullptr) task = _commonWaitingTaskQueue->pop();

    // If still no task was found (queues were empty), then return an empty task
    if (task == nullptr) return nullptr;

    // Checking for task's pending dependencies
    while (task->getDependencies().empty() == false)
    {
      // Checking whether the dependency is already finished
      const auto dependency = task->getDependencies().front();

      // If it is not finished:
      if (_finishedObjects.contains(dependency) == false) [[likely]]
      {
        // Push the task back into back of the queue
        _commonWaitingTaskQueue->push(task);

        // Return nullptr, signaling no task was found
        return nullptr;
      }

      // Otherwise, remove it out of the dependency queue
      task->getDependencies().pop();
    }

    // The task's dependencies may be satisfied, but now we got to check whether it has any pending operations
    while (task->getPendingOperations().empty() == false)
    {
      // Checking whether the operation has finished
      const auto pendingOperation = task->getPendingOperations().front();

      // Running pending operation checker
      const auto result = pendingOperation();

      // If not satisfied:
      if (result == false)
      {
        // Push the task back into back of the queue
        _commonWaitingTaskQueue->push(task);

        // Return nullptr, signaling no task was found
        return nullptr;
      }

      // Otherwise, remove it out of the dependency queue
      task->getPendingOperations().pop();
    }

    // Returning ready task
    return task;
  }

  __INLINE__ void onTaskExecuteCallback(HiCR::tasking::Task *task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    _taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskExecute);
  }

  __INLINE__ void onTaskFinishCallback(HiCR::tasking::Task *task)
  {
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
  }

  __INLINE__ void onTaskSuspendCallback(HiCR::tasking::Task *task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSuspend);
  }

  __INLINE__ void onTaskSyncCallback(HiCR::tasking::Task *task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If not defined, resume task (by default)
    if (this->_taskrCallbackMap.isCallbackSet(HiCR::tasking::Task::callback_t::onTaskSync) == false) _commonWaitingTaskQueue->push(taskrTask);

    // If defined, trigger user-defined event
    this->_taskrCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSync);
  }

  /**
   * A flag to indicate whether taskR was initialized
   */
  bool _isInitialized = false;

  /**
   * Pointer to the compute manager to use
   */
  HiCR::L1::ComputeManager *const _computeManager;

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
   * Set of workers assigned to execute tasks
   */
  std::vector<taskr::Worker *> _taskWorkers;

  /**
   * Set of workers assigned to execute services
   */
  std::vector<taskr::Worker *> _serviceWorkers;

  /**
   * Number of active (not suspended) workers
   */
  std::atomic<size_t> _activeTaskWorkerCount;

  /**
   * Counter for the current number of active tasks. Execution finishes when this counter reaches zero
   */
  std::atomic<size_t> _activeTaskCount = 0;

  /**
   * Common lock-free queue for waiting tasks.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _commonWaitingTaskQueue;

  /**
   * Lock-free queue storing workers that remain in suspension. Required for the max active workers mechanism
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Worker>> _suspendedTaskWorkerQueue;

  /**
   * The processing units assigned to taskr to run workers from
   */
  std::vector<std::unique_ptr<HiCR::L0::ProcessingUnit>> _processingUnits;

  /**
   * This parallel set stores the id of all finished objects
   */
  HiCR::concurrent::HashSet<HiCR::tasking::uniqueId_t> _finishedObjects;

  /**
   * Common lock-free queue for services.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::service_t>> _serviceQueue;

  //////// Configuration Elements

  /**
   * Time (ms) before a worker thread suspends after not finding any ready tasks
   */
  ssize_t _taskWorkerInactivityTimeMs;

  /**
   * Minimum of worker threads to keep active
   */
  size_t _minimumActiveTaskWorkers;

  /**
   * Numer of service workers to instantiate
   */
  size_t _serviceWorkerCount;

  /**
   * Whether the task workers also check the service queue (adds overhead but improves real-time event handling)
   */
  bool _makeTaskWorkersRunServices;

}; // class Runtime

} // namespace taskr
