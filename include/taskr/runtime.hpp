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
    _commonReadyTaskQueue   = std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_COMMON_ACTIVE_TASKS);
    _serviceQueue           = std::make_unique<HiCR::concurrent::Queue<taskr::service_t>>(__TASKR_DEFAULT_MAX_SERVICES);

    // Setting task callback functions
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskExecute, [this](HiCR::tasking::Task *task) { this->onTaskExecuteCallback(task); });
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [this](HiCR::tasking::Task *task) { this->onTaskFinishCallback(task); });
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSuspend, [this](HiCR::tasking::Task *task) { this->onTaskSuspendCallback(task); });
    _hicrCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSync, [this](HiCR::tasking::Task *task) { this->onTaskSyncCallback(task); });

    // Setting default services
    _serviceQueue->push(&checkOneWaitingTaskService);

    // Assigning configuration defaults
    _taskWorkerInactivityTimeMs      = 10;   // 10 ms for a task worker to suspend if it didn't find any suitable tasks to execute
    _taskWorkerSuspendIntervalTimeMs = 1;    // Worker will sleep for 1ms when suspended
    _minimumActiveTaskWorkers        = 1;    // Guarantee that there is at least one active task worker
    _serviceWorkerCount              = 0;    // No service workers (Typical setting for HPC applications)
    _makeTaskWorkersRunServices      = true; // Since no service workers are created by default, have task workers check on services

    // Parsing configuration
    if (config.contains("Task Worker Inactivity Time (Ms)")) _taskWorkerInactivityTimeMs = hicr::json::getNumber<ssize_t>(config, "Task Worker Inactivity Time (Ms)");
    if (config.contains("Task Suspend Interval Time (Ms)")) _taskWorkerSuspendIntervalTimeMs = hicr::json::getNumber<ssize_t>(config, "Task Suspend Interval Time (Ms)");
    if (config.contains("Minimum Active Task Workers")) _minimumActiveTaskWorkers = hicr::json::getNumber<size_t>(config, "Minimum Active Task Workers");
    if (config.contains("Service Worker Count")) _serviceWorkerCount = hicr::json::getNumber<size_t>(config, "Service Worker Count");
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
    // Push it into the common waiting task queue (its dependencies will be checked later)
    _commonWaitingTaskQueue->push(task);
  }

  /**
   * Initailizes the TaskR runtime
   * Creates a set of HiCR workers, based on the provided processing units and compute manager
   */
  __INLINE__ void initialize()
  {
    // Verify taskr is not currently initialized
    if (_isInitialized == true) HICR_THROW_LOGIC("Trying to initialize TaskR, but it is currently initialized");

    // Checking if we have at least one processing unit
    if (_processingUnits.empty()) HICR_THROW_LOGIC("Trying to initialize TaskR with no processing units assigned to it");

    // Checking if we have enough processing units
    if (_serviceWorkerCount >= _processingUnits.size())
      HICR_THROW_LOGIC("Trying to create equal or more service worker counts (%lu) than processing units (%lu) provided", _serviceWorkerCount, _processingUnits.size());

    // Initializing HiCR tasking
    HiCR::tasking::initialize();

    // Creating service workers, as specified by the configuration
    size_t serviceWorkerId = 0;
    for (size_t processingUnitId = 0; processingUnitId < _serviceWorkerCount; processingUnitId++)
    {
      // Creating new service worker
      auto serviceWorker = new taskr::Worker(_computeManager, [this, serviceWorkerId]() -> taskr::Task * { return serviceWorkerLoop(serviceWorkerId); });

      // Assigning resource to the thread
      serviceWorker->addProcessingUnit(std::move(_processingUnits[processingUnitId]));

      // Finally adding worker to the service worker set
      _serviceWorkers.push_back(serviceWorker);

      // Increasing service worker id
      serviceWorkerId++;
    }

    // Creating one task worker per remaining processung unit in the list after creating the service workers
    size_t taskWorkerId = 0;
    for (size_t processingUnitId = _serviceWorkerCount; processingUnitId < _processingUnits.size(); processingUnitId++)
    {
      // Creating new task worker
      auto taskWorker = new taskr::Worker(_computeManager, [this, taskWorkerId]() -> taskr::Task * { return taskWorkerLoop(taskWorkerId); });

      // Setting resume check function
      taskWorker->setCheckResumeFunction([this](taskr::Worker *worker) { return checkResumeWorker(worker); });

      // Setting suspension interval time
      taskWorker->setSuspendInterval(_taskWorkerSuspendIntervalTimeMs);

      // Assigning resource to the thread
      taskWorker->addProcessingUnit(std::move(_processingUnits[processingUnitId]));

      // Finally adding task worker to the task worker vector
      _taskWorkers.push_back(taskWorker);

      // Increasing task worker id
      taskWorkerId++;
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
    for (auto &w : _serviceWorkers) delete w;
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

    // Initializing workers
    for (auto &w : _serviceWorkers) w->initialize();
    for (auto &w : _taskWorkers) w->initialize();

    // Starting workers
    for (auto &w : _serviceWorkers) w->start();
    for (auto &w : _taskWorkers) w->start();

    // Waiting for workers to finish computing
    for (auto &w : _serviceWorkers) w->await();
    for (auto &w : _taskWorkers) w->await();
  }

  private:

  __INLINE__ taskr::Task *serviceWorkerLoop(const workerId_t serviceWorkerId)
  {
    // Getting worker pointer
    auto worker = _serviceWorkers[serviceWorkerId];

    // Checking for termination
    auto terminated = checkTermination(worker);

    // If terminated, return a null task immediately
    if (terminated == true) return nullptr;

    // Getting service (if any is available)
    auto service = _serviceQueue->pop();

    // If found run it, and put it back into the queue
    if (service != nullptr)
    {
      // Running service
      (*service)();

      // Putting it back into the queue
      _serviceQueue->push(service);
    }

    // Service threads run no tasks, so returning null
    return nullptr;
  }

  __INLINE__ taskr::Task *taskWorkerLoop(const workerId_t taskWorkerId)
  {
    // The worker is once again active
    _activeTaskWorkerCount++;

    // Getting worker pointer
    auto worker = _taskWorkers[taskWorkerId];

    // If required, perform a service task
    if (_makeTaskWorkersRunServices == true)
    {
      // Getting service (if any is available)
      auto service = _serviceQueue->pop();

      // If found run it, and put it back into the queue
      if (service != nullptr)
      {
        // Running service
        (*service)();

        // Putting it back into the queue
        _serviceQueue->push(service);
      }
    }

    // Getting next task to execute from the worker's own queue
    auto task = worker->getReadyTaskQueue()->pop();

    // If no task found, try to get one from the common ready queue
    if (task == nullptr) task = _commonReadyTaskQueue->pop();

    // If still no found was found set it as a failure to get useful job
    if (task == nullptr)
    {
      // Set the worker's fail time, if not already set
      worker->setFailedToRetrieveTask();

      // Check whether the conditions are met to put the worker to sleep due to inactivity
      checkTaskWorkerSuspension(worker);
    }

    // Check for termination
    checkTermination(worker);

    // The worker exits the main loop, therefore is no longer active
    _activeTaskWorkerCount--;

    // Returning task pointer regardless if found or not
    return task;
  }

  __INLINE__ bool checkTermination(taskr::Worker *worker)
  {
    // If all tasks finished, then terminate execution immediately
    if (_activeTaskCount == 0)
    {
      // Terminating worker.
      worker->terminate();

      // Return true (execution terminated)
      return true;
    }

    // Return false (execution still going)
    return false;
  }

  __INLINE__ bool checkResumeWorker(taskr::Worker *worker)
  {
    // There are not enough polling workers
    if (_activeTaskWorkerCount <= _minimumActiveTaskWorkers) return true;

    // The worker was asked to resume explicitly
    if (worker->getState() == taskr::Worker::state_t::resuming) return true;

    // The application has already finished
    if (_activeTaskCount == 0) return true;

    // The worker has pending tasks in its own ready task queue
    if (worker->getReadyTaskQueue()->wasEmpty() == false) return true;

    // There are pending ready tasks in the common queue
    if (_commonReadyTaskQueue->wasEmpty() == false) return true;

    // Return false (stay suspended)
    return false;
  }

  /**
   * Function to check whether the running thread needs to suspend
   */
  __INLINE__ void checkTaskWorkerSuspension(taskr::Worker *worker)
  {
    // Check for inactivity time (to put the worker to sleep)
    if (_taskWorkerInactivityTimeMs >= 0)               // If this setting is, negative then no suspension is used
      if (worker->getHasFailedToRetrieveTask() == true) // If the worker has failed to retrieve a task last time
        if (worker->getTimeSinceFailedToRetrievetaskMs() > (size_t)_taskWorkerInactivityTimeMs)
          if (_activeTaskWorkerCount > _minimumActiveTaskWorkers) // If we are already at the minimum, do not suspend.
            worker->suspend();
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
  __INLINE__ void checkOneWaitingTask()
  {
    // Poping task from the common waiting task queue
    auto task = _commonWaitingTaskQueue->pop();

    // If still no task was found (queue was empty), then return immediately
    if (task == nullptr) return;

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

        // Return immediately: task was not ready
        return;
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

        // Return immediately: task was not ready
        return;
      }

      // Otherwise, remove it out of the dependency queue
      task->getPendingOperations().pop();
    }

    // The task is ready to execute, add it to the corresponding queue

    // Getting task's affinity
    const auto taskAffinity = task->getWorkerAffinity();

    // Sanity Check
    if (taskAffinity >= (ssize_t)_taskWorkers.size())
      HICR_THROW_LOGIC("Invalid task affinity specified: %ld, which is larger than the largest worker id: %ld\n", taskAffinity, _taskWorkers.size() - 1);

    // If no affinity set, push it into the common task queue
    if (taskAffinity < 0) _commonReadyTaskQueue->push(task);

    // If the affinity was set, put it in the corresponding task worker's queue
    else
      _taskWorkers[taskAffinity]->resume();
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

    // Updating the thread activity time (so that it doesn't go to sleep immediately)
    const auto worker = (taskr::Worker *)taskr::Worker::getCurrentWorker();
    worker->resetRetrieveTaskSuccessFlag();

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
   * Number of polling workers
   * 
   * These are workers who can query the waiting task list and find new tasks
   */
  std::atomic<size_t> _pollingTaskWorkerCount;

  /**
   * Number of active (not suspended or busy executing a task) workers
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
   * Common lock-free queue for ready tasks.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _commonReadyTaskQueue;

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

  /**
   * Default service to check for waiting tasks's dependencies and pending operations to see if they are now ready
   */
  taskr::service_t checkOneWaitingTaskService = [this]() { this->checkOneWaitingTask(); };

  //////// Configuration Elements

  /**
   * Time (ms) before a worker thread suspends after not finding any ready tasks
   */
  ssize_t _taskWorkerInactivityTimeMs;

  /**
   * Time (ms) a worker will sleep for when suspended, in between checks whether the resume conditions are given
   */
  size_t _taskWorkerSuspendIntervalTimeMs;

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

  // A mutex for termination, required to have a single worker wait for others to finish
  std::mutex _terminationMutex;

}; // class Runtime

} // namespace taskr
