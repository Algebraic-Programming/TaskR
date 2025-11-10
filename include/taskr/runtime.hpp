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
#include <hicr/core/device.hpp>
#include <hicr/core/computeManager.hpp>
#include <hicr/frontends/tasking/common.hpp>
#include <hicr/frontends/tasking/tasking.hpp>

#ifdef ENABLE_INSTRUMENTATION
  #include <tracr.hpp>
#endif

#include "queue.hpp"
#include "task.hpp"
#include "taskImpl.hpp"
#include "worker.hpp"
#include "service.hpp"

namespace taskr
{

/**
 * Thread indices for the TraCR thread markers 
 */
struct ThreadIndices
{
  /**
   * thread idx exec_task
   */
  size_t exec_task;

  /**
   * thread idx exec_serv
   */
  size_t exec_serv;

  /**
   * thread idx polling
   */
  size_t polling;

  /**
   * thread idx suspending
   */
  size_t suspending;

  /**
   * thread idx resuming
   */
  size_t resuming;

  /**
   * thread idx finished
   */
  size_t finished;
};

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
   * @param[in] taskComputeManager A backend's compute manager to initialize and run the task's execution states.
   * @param[in] workerComputeManager A backend's compute manager to initialize and run processing units
   * @param[in] computeResources The compute resources to use to drive the workers
   * @param[in] config Optional configuration parameters passed in JSON format
   */
  Runtime(HiCR::ComputeManager *const               taskComputeManager,
          HiCR::ComputeManager *const               workerComputeManager,
          const HiCR::Device::computeResourceList_t computeResources,
          nlohmann::json                            config = nlohmann::json())
    : _taskComputeManager(taskComputeManager),
      _workerComputeManager(workerComputeManager),
      _computeResources(computeResources)
  {
#ifdef ENABLE_INSTRUMENTATION
    // This is to check if ovni has been already initialized by nOS-V
    bool external_init_ = (dynamic_cast<HiCR::backend::pthreads::ComputeManager *>(_workerComputeManager) == nullptr) ? true : false;

    // TraCR start tracing
    INSTRUMENTATION_START(external_init_);

    // TraCR initialize marker type
    INSTRUMENTATION_THREAD_MARK_INIT(0);

    // TraCR marker types with the given string messages
    thread_idx.exec_task  = INSTRUMENTATION_THREAD_MARK_ADD(MARK_COLOR_GREEN, "executing");
    thread_idx.exec_serv  = INSTRUMENTATION_THREAD_MARK_ADD(MARK_COLOR_CYAN, "executing a service");
    thread_idx.polling    = INSTRUMENTATION_THREAD_MARK_ADD(MARK_COLOR_NAVY, "polling");
    thread_idx.suspending = INSTRUMENTATION_THREAD_MARK_ADD(MARK_COLOR_LIGHT_GRAY, "suspended");
    thread_idx.resuming   = INSTRUMENTATION_THREAD_MARK_ADD(MARK_COLOR_LIGHT_GREEN, "resumed");
    thread_idx.finished   = INSTRUMENTATION_THREAD_MARK_ADD(MARK_COLOR_YELLOW, "finished");
#endif

    // Creating internal tasks
    _commonReadyTaskQueue = std::make_unique<HiCR::concurrent::Queue<taskr::Task>>(__TASKR_DEFAULT_MAX_COMMON_ACTIVE_TASKS);
    _serviceQueue         = std::make_unique<HiCR::concurrent::Queue<taskr::Service>>(__TASKR_DEFAULT_MAX_SERVICES);

    // Setting task callback functions
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskExecute, [this](HiCR::tasking::Task *task) { this->onTaskExecuteCallback(task); });
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [this](HiCR::tasking::Task *task) { this->onTaskFinishCallback(task); });
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSuspend, [this](HiCR::tasking::Task *task) { this->onTaskSuspendCallback(task); });
    _hicrTaskCallbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskSync, [this](HiCR::tasking::Task *task) { this->onTaskSyncCallback(task); });

    // Setting ServiceWorker callback functions
    _serviceWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerStart, [this](HiCR::tasking::Worker *worker) { this->onWorkerStartCallback(worker); });
    _serviceWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerSuspend, [this](HiCR::tasking::Worker *worker) { this->onWorkerSuspendCallback(worker); });
    _serviceWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerResume, [this](HiCR::tasking::Worker *worker) { this->onWorkerResumeCallback(worker); });
    _serviceWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerTerminate, [this](HiCR::tasking::Worker *worker) { this->onWorkerTerminateCallback(worker); });

    // Setting TaskWorker callback functions
    _taskWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerStart, [this](HiCR::tasking::Worker *worker) { this->onWorkerStartCallback(worker); });
    _taskWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerSuspend, [this](HiCR::tasking::Worker *worker) { this->onWorkerSuspendCallback(worker); });
    _taskWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerResume, [this](HiCR::tasking::Worker *worker) { this->onWorkerResumeCallback(worker); });
    _taskWorkerCallbackMap.setCallback(HiCR::tasking::Worker::callback_t::onWorkerTerminate, [this](HiCR::tasking::Worker *worker) { this->onWorkerTerminateCallback(worker); });

    // Assigning configuration defaults
    _taskWorkerInactivityTimeMs      = 10;    // 10 ms for a task worker to suspend if it didn't find any suitable tasks to execute
    _taskWorkerSuspendIntervalTimeMs = 1;     // Worker will sleep for 1ms when suspended
    _minimumActiveTaskWorkers        = 1;     // Guarantee that there is at least one active task worker
    _serviceWorkerCount              = 0;     // No service workers (Typical setting for HPC applications)
    _makeTaskWorkersRunServices      = false; // Since no service workers are created by default, have task workers check on services
    _finishOnLastTask                = true;  // Indicates that taskR must finish when the last task has finished

    // Parsing configuration
    if (config.contains("Task Worker Inactivity Time (Ms)")) _taskWorkerInactivityTimeMs = hicr::json::getNumber<ssize_t>(config, "Task Worker Inactivity Time (Ms)");
    if (config.contains("Task Suspend Interval Time (Ms)")) _taskWorkerSuspendIntervalTimeMs = hicr::json::getNumber<ssize_t>(config, "Task Suspend Interval Time (Ms)");
    if (config.contains("Minimum Active Task Workers")) _minimumActiveTaskWorkers = hicr::json::getNumber<size_t>(config, "Minimum Active Task Workers");
    if (config.contains("Service Worker Count")) _serviceWorkerCount = hicr::json::getNumber<size_t>(config, "Service Worker Count");
    if (config.contains("Make Task Workers Run Services")) _makeTaskWorkersRunServices = hicr::json::getBoolean(config, "Make Task Workers Run Services");
    if (config.contains("Finish on Last Task")) _finishOnLastTask = hicr::json::getBoolean(config, "Finish On Last Task");

    // Initial state
    _activeTaskCount = 0;
  }

  // Destructor
  ~Runtime() {}

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
    resumeTask(task);
  }

  /**
   * Re-activates (resumes) a task by adding it back to the waiting task queue
   *
   * \param[in] task Task to resume.
   */
  __INLINE__ void resumeTask(taskr::Task *const task)
  {
    // Checking that the task is ready to be resumed at this point
    auto dependencyCount = task->getDependencyCount();
    if (dependencyCount > 0) return;

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
      auto serviceWorker = std::make_shared<taskr::Worker>(
        serviceWorkerId, _taskComputeManager, _workerComputeManager, [this, serviceWorkerId]() -> taskr::Task * { return serviceWorkerLoop(serviceWorkerId); });

      // Making sure the worker has its callback map correctly assigned
      serviceWorker->setCallbackMap(&_serviceWorkerCallbackMap);

      // Assigning resource to the thread
      serviceWorker->addProcessingUnit(_workerComputeManager->createProcessingUnit(_computeResources[computeResourceId]));

      // Finally adding worker to the service worker set
      _serviceWorkers.push_back(serviceWorker);

      // Increasing service worker id
      serviceWorkerId++;
    }

    // Creating one task worker per remaining processung unit in the list after creating the service workers
    size_t taskWorkerId = 0;
    for (size_t computeResourceId = _serviceWorkerCount; computeResourceId < _computeResources.size(); computeResourceId++)
    {
      // // Getting up-casted pointer for the processing unit
      // auto c = dynamic_pointer_cast<HiCR::backend::hwloc::ComputeResource>(_computeResources[computeResourceId]);

      // // Checking whether the execution unit passed is compatible with this backend
      // if (c == nullptr) HICR_THROW_LOGIC("The passed compute resource is not supported by this processing unit type\n");

      // // Getting the logical processor ID of the compute resource
      // auto pid = c->getProcessorId();
      // printf("activating PU with PID: %d\n", pid);

      // Creating new task worker
      auto taskWorker =
        std::make_shared<taskr::Worker>(taskWorkerId, _taskComputeManager, _workerComputeManager, [this, taskWorkerId]() -> taskr::Task * { return taskWorkerLoop(taskWorkerId); });

      // Making sure the worker has its callback map correctly assigned
      taskWorker->setCallbackMap(&_taskWorkerCallbackMap);

      // Setting resume check function
      taskWorker->setCheckResumeFunction([this](taskr::Worker *worker) { return checkResumeWorker(worker); });

      // Setting suspension interval time
      taskWorker->setSuspendInterval(_taskWorkerSuspendIntervalTimeMs);

      // Assigning resource to the thread
      taskWorker->addProcessingUnit(_workerComputeManager->createProcessingUnit(_computeResources[computeResourceId]));

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
    // Clear force terminate flag
    _forceTerminate = false;

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

#ifdef ENABLE_INSTRUMENTATION
    // TraCR set trace of the main thread being finished
    INSTRUMENTATION_THREAD_MARK_SET(thread_idx.finished);
#endif
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

    // Clearing created workers
    _serviceWorkers.clear();
    _taskWorkers.clear();

    // Setting state back to uninitialized
    _state = state_t::uninitialized;

#ifdef ENABLE_INSTRUMENTATION
    // TraCR stop tracing
    INSTRUMENTATION_END();
#endif
  }

  /**
   * This function informs TaskR that a certain task (with a given unique ID) has finished
   * If this task the last remaining dependency for a given task, now the task may be scheduled for execution.
   * 
   * @param[in] task The task to report as finished
   */
  __INLINE__ void setFinishedTask(taskr::Task *const task)
  {
    // Now for each task that dependends on this task, reduce their dependencies by one
    for (auto &dependentTask : task->getOutputDependencies())
    {
      // Removing task's dependency
      auto remainingDependencies = dependentTask->decrementDependencyCount();

      // If the task has no remaining dependencies, continue executing it
      if (remainingDependencies == 0) resumeTask(dependentTask);
    }
  }

  /**
   * Adds a service to be executed regularly by service workers
   * 
   * @param[in] service The service (function) to add
   */
  __INLINE__ void addService(taskr::Service *service) { _serviceQueue->push(service); }

  /**
   * Funtion to force termination in case the application does not have its own termination logic
   */
  __INLINE__ void forceTermination() { _forceTerminate = true; }

  /**
   * Function to toggle the finish on last task condition
   * 
   * @param[in] value True, if taskr must finish when the last active task finishes; false, if it needs to continue running otherwise
   */
  __INLINE__ void setFinishOnLastTask(const bool value = true) { _finishOnLastTask = value; }

  /**
   * Function to check whether there are active tasks remaining
   * 
   * @return The number of tasks current active (running or pending)
   */
  __INLINE__ size_t getActiveTaskCounter() const { return _activeTaskCount; }

  /**
   * Function to get the compute manager specified for the creation of execution states (tasks)
   * 
   * @return The compute manager assigned for the creation of task states
   */
  __INLINE__ HiCR::ComputeManager *getTaskComputeManager() const { return _taskComputeManager; }

  /**
   * Function to get the compute manager specified for the creation of processing units (workers)
   * 
   * @return The compute manager assigned for the management of workers
   */
  __INLINE__ HiCR::ComputeManager *getWorkerComputeManager() const { return _workerComputeManager; }

  private:

  __INLINE__ void tryRunService(Service *const service)
  {
    // Checking if service is enabled
    if (service->isEnabled())
    {
      // Checking if service is active (or inactive, i.e.,  waiting for its wait interval to pass)
      if (service->isActive())
      {
// TraCR set trace of thread executing a service
#ifdef ENABLE_INSTRUMENTATION
        INSTRUMENTATION_THREAD_MARK_SET(thread_idx.exec_serv);
#endif

        // Now run service
        service->run();
      }
    }
  }

  __INLINE__ taskr::Task *serviceWorkerLoop(const workerId_t serviceWorkerId)
  {
#ifdef ENABLE_INSTRUMENTATION
    // TraCR set trace of thread polling
    INSTRUMENTATION_THREAD_MARK_SET(thread_idx.polling);
#endif

    // Getting worker pointer
    auto worker = _serviceWorkers[serviceWorkerId];

    // Checking for termination
    auto terminated = checkTermination(worker.get());

    // If terminated, return a null task immediately
    if (terminated == true) return nullptr;

    // Getting service (if any is available)
    auto service = _serviceQueue->pop();

    // If found run it, and put it back into the queue
    if (service != nullptr)
    {
      // Try to run service
      tryRunService(service);

      // Putting it back into the queue
      _serviceQueue->push(service);
    }

    // Service threads run no tasks, so returning null
    return nullptr;
  }

  __INLINE__ taskr::Task *taskWorkerLoop(const workerId_t taskWorkerId)
  {
#ifdef ENABLE_INSTRUMENTATION
    // TraCR set trace of thread polling
    INSTRUMENTATION_THREAD_MARK_SET(thread_idx.polling);
#endif

    // The worker is once again active
    _activeTaskWorkerCount++;

    // Getting worker pointer
    auto worker = _taskWorkers[taskWorkerId];

    // If force termination is set, clearing all tasks from the general and worker's queue
    if (_forceTerminate == true)
    {
      while (_commonReadyTaskQueue->wasEmpty() == false)
      {
        auto task = _commonReadyTaskQueue->pop();
        if (task != nullptr) _activeTaskCount--;
      }

      while (worker->getReadyTaskQueue()->wasEmpty() == false)
      {
        auto task = worker->getReadyTaskQueue()->pop();
        if (task != nullptr) _activeTaskCount--;
      }
    }

    // If required, perform a service task
    if (_makeTaskWorkersRunServices == true)
    {
      // Getting service (if any is available)
      auto service = _serviceQueue->pop();

      // If found run it, and put it back into the queue
      if (service != nullptr)
      {
        // Try to run service
        tryRunService(service);

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
      checkTaskWorkerSuspension(worker.get());
    }

    // If task was found, set it as a success (to prevent the worker from going to sleep)
    if (task != nullptr) worker->resetRetrieveTaskSuccessFlag();

    // Check for termination
    if (task == nullptr) checkTermination(worker.get());

    // The worker exits the main loop, therefore is no longer active
    _activeTaskWorkerCount--;

    // Returning task pointer regardless if found or not
    return task;
  }

  __INLINE__ bool checkTermination(taskr::Worker *const worker)
  {
    // If all tasks finished and the runtime is set to finish on that condition, then terminate execution immediately
    if (_finishOnLastTask == true && _activeTaskCount == 0)
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
          if (_activeTaskWorkerCount > _minimumActiveTaskWorkers) worker->suspend(); // If we are already at the minimum, do not suspend.
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

    // Setting task as finished task
    setFinishedTask(taskrTask);

    // If defined, trigger user-defined event
    this->_taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskFinish);

    // Decreasing active task counter
    _activeTaskCount--;
  }

  __INLINE__ void onTaskSuspendCallback(HiCR::tasking::Task *const task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    this->_taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSuspend);
  }

  __INLINE__ void onTaskSyncCallback(HiCR::tasking::Task *const task)
  {
    // Getting TaskR task pointer
    auto taskrTask = (taskr::Task *)task;

    // If defined, trigger user-defined event
    this->_taskCallbackMap.trigger(taskrTask, HiCR::tasking::Task::callback_t::onTaskSync);
  }

  __INLINE__ void onWorkerStartCallback(HiCR::tasking::Worker *const worker)
  {
#ifdef ENABLE_INSTRUMENTATION
    // TraCR initialize the thread
    INSTRUMENTATION_THREAD_INIT();
#endif

    // Getting TaskR worker pointer
    auto taskrWorker = (taskr::Worker *)worker;

    // If defined, trigger user-defined event
    this->_workerCallbackMap.trigger(taskrWorker, HiCR::tasking::Worker::callback_t::onWorkerStart);
  }

  __INLINE__ void onWorkerSuspendCallback(HiCR::tasking::Worker *const worker)
  {
    // Getting TaskR worker pointer
    auto taskrWorker = (taskr::Worker *)worker;

#ifdef ENABLE_INSTRUMENTATION
    // TraCR set trace of thread suspended
    INSTRUMENTATION_THREAD_MARK_SET(thread_idx.suspending);
#endif

    // If defined, trigger user-defined event
    this->_workerCallbackMap.trigger(taskrWorker, HiCR::tasking::Worker::callback_t::onWorkerSuspend);
  }

  __INLINE__ void onWorkerResumeCallback(HiCR::tasking::Worker *const worker)
  {
    // Getting TaskR worker pointer
    auto taskrWorker = (taskr::Worker *)worker;

#ifdef ENABLE_INSTRUMENTATION
    // TraCR set trace of thread resumed
    INSTRUMENTATION_THREAD_MARK_SET(thread_idx.resuming);
#endif

    // If defined, trigger user-defined event
    this->_workerCallbackMap.trigger(taskrWorker, HiCR::tasking::Worker::callback_t::onWorkerResume);
  }

  __INLINE__ void onWorkerTerminateCallback(HiCR::tasking::Worker *const worker)
  {
    // Getting TaskR worker pointer
    auto taskrWorker = (taskr::Worker *)worker;

#ifdef ENABLE_INSTRUMENTATION
    // Set the marker of this thread to be finished
    INSTRUMENTATION_THREAD_MARK_SET(thread_idx.finished);

    // TraCR end thread (only if backend is not nOS-V)
    INSTRUMENTATION_THREAD_END();
#endif

    // If defined, trigger user-defined event
    this->_workerCallbackMap.trigger(taskrWorker, HiCR::tasking::Worker::callback_t::onWorkerTerminate);
  }

  /**
   * Flag to indicate whether execution must be forcibly terminated. It is discouraged to use this if the application
   * has implemented a clear ending logic. This is only useful for always-on services-like applications
   */
  bool _forceTerminate;

  /**
   * A flag to indicate whether taskR was initialized
   */
  state_t _state = state_t::uninitialized;

  /**
   * Compute manager to use to instantiate task's execution states
   */
  HiCR::ComputeManager *const _taskComputeManager;

  /**
   * Compute manager to use to instantiate processing units
   */
  HiCR::ComputeManager *const _workerComputeManager;

  /**
   * Type definition for the task's callback map
   */
  typedef HiCR::tasking::CallbackMap<taskr::Task *, HiCR::tasking::Task::callback_t> taskCallbackMap_t;

  /**
   *  HiCR callback map shared by all tasks
   */
  HiCR::tasking::Task::taskCallbackMap_t _hicrTaskCallbackMap;

  /**
   *  TaskR-specific task callmap, customizable by the user
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
   *  TaskR-specific worker callmap, customizable by the user
   */
  workerCallbackMap_t _workerCallbackMap;

  /**
   * Set of workers assigned to execute tasks
   */
  std::vector<std::shared_ptr<taskr::Worker>> _taskWorkers;

  /**
   * Set of workers assigned to execute services
   */
  std::vector<std::shared_ptr<taskr::Worker>> _serviceWorkers;

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
  std::atomic<size_t> _activeTaskCount;

  /**
   * Common lock-free queue for ready tasks.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Task>> _commonReadyTaskQueue;

  /**
   * The compute resources to use to run workers with
   */
  HiCR::Device::computeResourceList_t _computeResources;

  /**
   * Common lock-free queue for services.
   */
  std::unique_ptr<HiCR::concurrent::Queue<taskr::Service>> _serviceQueue;

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

  /**
   * Indicates whether taskR must finish when the last task has finished
   */
  bool _finishOnLastTask;

  /**
   * TraCR thread indices
   */
  ThreadIndices thread_idx;
}; // class Runtime

} // namespace taskr
