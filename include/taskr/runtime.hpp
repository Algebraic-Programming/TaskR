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
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/pthreads/L1/computeManager.hpp>
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/tasking.hpp>
#include <hicr/core/concurrent/queue.hpp>
#include <hicr/core/concurrent/hashMap.hpp>
#include <hicr/core/concurrent/hashSet.hpp>
#include "task.hpp"
#include "taskImpl.hpp"
#include "worker.hpp"

namespace taskr
{

/**
 * Enumeration of states in which the TaskR runtime can be in
 */
enum state_t
{
  /**
    * The runtime has not yet been initialized
    */
  uninitialized,

  /**
    * The runtime is initialized, but not running
    */
  initialized,

  /**
    * The runtime is currently running
    */
  running
};

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
   * @param[in] computeResources The compute resources to use to drive the workers
   * @param[in] config Optional configuration parameters passed in JSON format
   */
  Runtime(const HiCR::L0::Device::computeResourceList_t computeResources, nlohmann::json config = nlohmann::json())
    : _computeResources(computeResources)
  {
    // Creating internal objects
    _commonReadyTaskQueue = std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_COMMON_ACTIVE_TASKS);
    _serviceQueue         = std::make_unique<HiCR::concurrent::Queue<taskr::service_t>>(__TASKR_DEFAULT_MAX_SERVICES);

    // Setting task callback functions
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskExecute, [this](HiCR::tasking::Task *task) { this->onTaskExecuteCallback(task); });
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [this](HiCR::tasking::Task *task) { this->onTaskFinishCallback(task); });
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSuspend, [this](HiCR::tasking::Task *task) { this->onTaskSuspendCallback(task); });
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSync, [this](HiCR::tasking::Task *task) { this->onTaskSyncCallback(task); });

    // Assigning configuration defaults
    _taskWorkerInactivityTimeMs      = 10;    // 10 ms for a task worker to suspend if it didn't find any suitable tasks to execute
    _taskWorkerSuspendIntervalTimeMs = 1;     // Worker will sleep for 1ms when suspended
    _minimumActiveTaskWorkers        = 1;     // Guarantee that there is at least one active task worker
    _serviceWorkerCount              = 0;     // No service workers (Typical setting for HPC applications)
    _makeTaskWorkersRunServices      = false; // Since no service workers are created by default, have task workers check on services

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
  HiCR::L1::ComputeManager *getComputeManager() { return &_computeManager; }

  ///////////// Local tasking API

  /**
   * Adds a callback for a particular task event (e.g., on starting, suspending, finishing)
   *
   * \param[in] event The task callback event to assin the callback to
   * \param[in] fc The callback function to call when the event is triggered
   */
  __INLINE__ void setTaskCallbackHandler(const HiCR::tasking::Task::callback_t event, HiCR::tasking::callbackFc_t<taskr::Task *> fc) { _taskCallbackMap.setCallback(event, fc); }

  /**
   * Adds a callback for a particular service worker event (e.g., on starting, finishing)
   *
   * \param[in] event The worker callback event to assin the callback to
   * \param[in] fc The callback function to call when the event is triggered
   */
  __INLINE__ void setServiceWorkerCallbackHandler(const HiCR::tasking::Worker::callback_t event, HiCR::tasking::callbackFc_t<HiCR::tasking::Worker *> fc)
  {
    _serviceWorkerCallbackMap.setCallback(event, fc);
  }

  /**
   * Adds a callback for a particular task worker event (e.g., on starting, suspend, resume, finishing)
   *
   * \param[in] event The worker callback event to assin the callback to
   * \param[in] fc The callback function to call when the event is triggered
   */
  __INLINE__ void setTaskWorkerCallbackHandler(const HiCR::tasking::Worker::callback_t event, HiCR::tasking::callbackFc_t<HiCR::tasking::Worker *> fc)
  {
    _taskWorkerCallbackMap.setCallback(event, fc);
  }

  /**
   * Adds a task to the TaskR runtime for future execution. This can be called at any point, before or during the execution of TaskR.
   *
   * \param[in] task Task to add.
   */
  __INLINE__ void addTask(taskr::Task *const task)
  {
    // Increasing active task counter
    _activeTaskCount++;

    // Making sure the task has its callback map correctly assigned
    task->setCallbackMap(&_hicrTaskCallbackMap);

    // Add task to the common waiting queue
    if (task->getDependencyCount() == 0) resumeTask(task);
  }

  /**
   * Re-activates (resumes) a task by adding it back to the waiting task queue
   *
   * \param[in] task Task to resume.
   */
  __INLINE__ void resumeTask(taskr::Task *const task)
  {
    // Getting task's affinity
    const auto taskAffinity = task->getWorkerAffinity();

    // Sanity Check
    if (taskAffinity >= (ssize_t)_taskWorkers.size())
      HICR_THROW_LOGIC("Invalid task affinity specified: %ld, which is larger than the largest worker id: %ld\n", taskAffinity, _taskWorkers.size() - 1);

    // If affinity set,
    if (taskAffinity >= 0)
    {
      // Push it into the worker's own task queue
      _taskWorkers[taskAffinity]->getReadyTaskQueue()->push(task);

      // Just in case it was asleep, awaken worker
      _taskWorkers[taskAffinity]->resume();
    }
    else // add to the common ready task queue
    {
      _commonReadyTaskQueue->push(task);
    }
  }

  /**
   * Adds a dependency on a given label for the specified task
   *
   * \param[in] task Task which to add a dependency
   * \param[in] dependency The label representing the dependency
   */
  __INLINE__ void addDependency(taskr::Task *const task, const label_t dependency)
  {
    // Increasing the atomic dependency counter in the task
    task->addDependency();

    // Register it also as an output dependency for notification later
    _outputDependencies[dependency].insert(task);
  }

  /**
   * Initailizes the TaskR runtime
   * Creates a set of HiCR workers, based on the provided processing units and compute manager
   */
  __INLINE__ void initialize()
  {
    // Verify taskr is not currently initialized
    if (_state != state_t::uninitialized) HICR_THROW_LOGIC("Trying to initialize TaskR, but it is currently initialized");

    // Checking if we have at least one processing unit
    if (_computeResources.empty()) HICR_THROW_LOGIC("Trying to initialize TaskR with no processing units assigned to it");

    // Checking if we have enough processing units
    if (_serviceWorkerCount >= _computeResources.size())
      HICR_THROW_LOGIC("Trying to create equal or more service worker counts (%lu) than processing units (%lu) provided", _serviceWorkerCount, _computeResources.size());

    // Creating service workers, as specified by the configuration
    size_t serviceWorkerId = 0;
    for (size_t computeResourceId = 0; computeResourceId < _serviceWorkerCount; computeResourceId++)
    {
      // Creating new service worker
      auto serviceWorker = new taskr::Worker(&_computeManager, [this, serviceWorkerId]() -> taskr::Task * { return serviceWorkerLoop(serviceWorkerId); });

      // Making sure the worker has its callback map correctly assigned
      serviceWorker->setCallbackMap(&_serviceWorkerCallbackMap);

      // Assigning resource to the thread
      serviceWorker->addProcessingUnit(_computeManager.createProcessingUnit(_computeResources[computeResourceId]));

      // Finally adding worker to the service worker set
      _serviceWorkers.push_back(serviceWorker);

      // Increasing service worker id
      serviceWorkerId++;
    }

    // Creating one task worker per remaining processung unit in the list after creating the service workers
    size_t taskWorkerId = 0;
    for (size_t computeResourceId = _serviceWorkerCount; computeResourceId < _computeResources.size(); computeResourceId++)
    {
      // Creating new task worker
      auto taskWorker = new taskr::Worker(&_computeManager, [this, taskWorkerId]() -> taskr::Task * { return taskWorkerLoop(taskWorkerId); });

      // Making sure the worker has its callback map correctly assigned
      taskWorker->setCallbackMap(&_taskWorkerCallbackMap);

      // Setting resume check function
      taskWorker->setCheckResumeFunction([this](taskr::Worker *worker) { return checkResumeWorker(worker); });

      // Setting suspension interval time
      taskWorker->setSuspendInterval(_taskWorkerSuspendIntervalTimeMs);

      // Assigning resource to the thread
      taskWorker->addProcessingUnit(_computeManager.createProcessingUnit(_computeResources[computeResourceId]));

      // Finally adding task worker to the task worker vector
      _taskWorkers.push_back(taskWorker);

      // Increasing task worker id
      taskWorkerId++;
    }

    // Setting taskr as uninitialized
    _state = state_t::initialized;
  }

  /**
   * Starts the execution of the TaskR runtime.
   * It starts the workers and suspends the current context until they're back (all tasks have finished).
   */
  __INLINE__ void run()
  {
    // Verify taskr is correctly initialized and not running
    if (_state == state_t::uninitialized) HICR_THROW_LOGIC("Trying to run TaskR, but it was not initialized");
    if (_state == state_t::running) HICR_THROW_LOGIC("Trying to run TaskR, but it is currently running");

    // Initializing workers
    for (auto &w : _serviceWorkers) w->initialize();
    for (auto &w : _taskWorkers) w->initialize();

    // Starting workers
    for (auto &w : _serviceWorkers) w->start();
    for (auto &w : _taskWorkers) w->start();

    // Set state to running
    _state = state_t::running;
  }

  /**
   * Awaits for the finalization of the current execution of the TaskR runtime.
   */
  __INLINE__ void await()
  {
    // Verify taskr is correctly running
    if (_state != state_t::running) HICR_THROW_LOGIC("Trying to wait for TaskR, but it was not running");

    // Waiting for workers to finish computing
    for (auto &w : _serviceWorkers) w->await();
    for (auto &w : _taskWorkers) w->await();

    // Set state back to initialized
    _state = state_t::initialized;
  }

  /**
   * Finalizes the TaskR runtime
   * Releases all workers and frees up their memory
   */
  __INLINE__ void finalize()
  {
    // Verify taskr is currently initialized
    if (_state == state_t::uninitialized) HICR_THROW_LOGIC("Trying to finalize TaskR, but it is currently not initialized");
    if (_state == state_t::running) HICR_THROW_LOGIC("Trying to finalize TaskR, but it is currently running. You need to run 'await' first to make sure it has stopped.");

    // Clearing created objects
    for (auto &w : _serviceWorkers) delete w;
    for (auto &w : _taskWorkers) delete w;
    _taskWorkers.clear();

    // Setting state back to uninitialized
    _state = state_t::uninitialized;
  }

  /**
   * This function informs TaskR that a certain object (with a given unique label) has finished
   * If this object the last remaining dependency for a given task, now the task may be scheduled for execution.
   * 
   * @param[in] object Label of the object to report as finished
   */
  __INLINE__ void setFinishedObject(const HiCR::tasking::uniqueId_t object)
  {
    for (auto &task : _outputDependencies[object])
    {
      // Removing task's dependency
      auto remainingDependencies = task->removeDependency() - 1;

      // If the task has no remaining dependencies, continue executing it
      if (remainingDependencies == 0) resumeTask(task);
    }
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

    // If no task found, check the comment ready task queue
    if (task == nullptr) task = _commonReadyTaskQueue->pop();

    // The task's dependencies may be satisfied, but now we got to check whether it has any pending operations
    if (task != nullptr)
      while (task->getPendingOperations().empty() == false)
      {
        // Checking whether the operation has finished
        const auto pendingOperation = task->getPendingOperations().front();

        // Running pending operation checker
        const auto result = pendingOperation();

        // If not satisfied, return task to the appropriate queue, set it task as nullptr (no task), and break cycle
        if (result == false) [[likely]]
        {
          resumeTask(task);
          task = nullptr;
          break;
        }

        // Otherwise, remove it out of the dependency queue
        task->getPendingOperations().pop_front();
      }

    // If still no found was found set it as a failure to get useful job
    if (task == nullptr)
    {
      // Set the worker's fail time, if not already set
      worker->setFailedToRetrieveTask();

      // Check whether the conditions are met to put the worker to sleep due to inactivity
      checkTaskWorkerSuspension(worker);
    }

    // If task was found, set it as a success (to prevent the worker from going to sleep)
    if (task != nullptr) worker->resetRetrieveTaskSuccessFlag();

    // Making the task dependent in its own execution to prevent it from re-running later
    if (task != nullptr) task->addDependency();

    // Check for termination
    if (task == nullptr) checkTermination(worker);

    // The worker exits the main loop, therefore is no longer active
    _activeTaskWorkerCount--;

    // Returning task pointer regardless if found or not
    return task;
  }

  __INLINE__ bool checkTermination(taskr::Worker *const worker)
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

  __INLINE__ bool checkResumeWorker(taskr::Worker *const worker)
  {
    // There are not enough polling workers
    if (_activeTaskWorkerCount <= _minimumActiveTaskWorkers) return true;

    // The worker was asked to resume explicitly
    if (worker->getState() == taskr::Worker::state_t::resuming) return true;

    // The application has already finished
    if (_activeTaskCount == 0) return true;

    // The worker has pending tasks in its own ready task queue
    if (worker->getReadyTaskQueue()->wasEmpty() == false) return true;

    // Return false (stay suspended)
    return false;
  }

  /**
   * Function to check whether the running thread needs to suspend
   */
  __INLINE__ void checkTaskWorkerSuspension(taskr::Worker *const worker)
  {
    // Check for inactivity time (to put the worker to sleep)
    if (_taskWorkerInactivityTimeMs >= 0)               // If this setting is, negative then no suspension is used
      if (worker->getHasFailedToRetrieveTask() == true) // If the worker has failed to retrieve a task last time
        if (worker->getTimeSinceFailedToRetrievetaskMs() > (size_t)_taskWorkerInactivityTimeMs)
          if (_activeTaskWorkerCount > _minimumActiveTaskWorkers) // If we are already at the minimum, do not suspend.
            worker->suspend();
  }

  __INLINE__ void onTaskExecuteCallback(HiCR::tasking::Task *const task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    _taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskExecute);
  }

  __INLINE__ void onTaskFinishCallback(HiCR::tasking::Task *const task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // Setting task as finished object
    setFinishedObject(taskrTask->getLabel());

    // If defined, trigger user-defined event
    this->_taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskFinish);

    // Removing entry from output dependency map
    _outputDependencies.erase(taskrTask->getLabel());

    // Decreasing active task counter
    _activeTaskCount--;
  }

  __INLINE__ void onTaskSuspendCallback(HiCR::tasking::Task *const task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    this->_taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSuspend);

    // Removing task's dependency on itself
    auto remainingDependencies = taskrTask->removeDependency() - 1;

    // If there are no remaining dependencies, adding task to ready task list
    if (remainingDependencies == 0) resumeTask(taskrTask);
  }

  __INLINE__ void onTaskSyncCallback(HiCR::tasking::Task *const task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    this->_taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSync);

    // Removing task's dependency on itself
    auto remainingDependencies = taskrTask->removeDependency() - 1;

    // If there are no remaining dependencies, adding task to ready task list
    if (remainingDependencies == 0) resumeTask(taskrTask);
  }

  /**
   * A flag to indicate whether taskR was initialized
   */
  state_t _state = state_t::uninitialized;

  /**
   * Pointer to the compute manager to use
   */
  HiCR::backend::host::pthreads::L1::ComputeManager _computeManager;

  /**
   * Type definition for the task's callback map
   */
  typedef HiCR::tasking::CallbackMap<taskr::Task *, HiCR::tasking::Task::callback_t> taskCallbackMap_t;

  /**
   *  HiCR callback map shared by all tasks
   */
  HiCR::tasking::Task::taskCallbackMap_t _hicrTaskCallbackMap;

  /**
   *  TaskR-specific callmap, customizable by the user
   */
  taskCallbackMap_t _taskCallbackMap;

  /**
   * Type definition for the worker's callback map
   */
  typedef HiCR::tasking::CallbackMap<taskr::Worker *, HiCR::tasking::Worker::callback_t> workerCallbackMap_t;

  /**
   *  HiCR callback map shared by all service workers
   */
  HiCR::tasking::Worker::workerCallbackMap_t _serviceWorkerCallbackMap;

  /**
   *  HiCR callback map shared by all task workers
   */
  HiCR::tasking::Worker::workerCallbackMap_t _taskWorkerCallbackMap;

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
   * Common lock-free queue for ready tasks.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _commonReadyTaskQueue;

  /**
   * Map for output dependencies
   */
  HiCR::concurrent::HashMap<taskr::label_t, HiCR::concurrent::HashSet<taskr::Task *>> _outputDependencies;

  /**
   * The compute resources to use to run workers with
   */
  HiCR::L0::Device::computeResourceList_t _computeResources;

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
}; // class Runtime

} // namespace taskr
