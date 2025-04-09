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

#include <chrono>
#include <hwloc.h>
#include <taskr/taskr.hpp>
#include <hicr/core/L1/communicationManager.hpp>
#include <hicr/core/L1/instanceManager.hpp>
#include <hicr/core/L1/memoryManager.hpp>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <hicr/backends/pthreads/L1/computeManager.hpp>
#include <hicr/backends/boost/L1/computeManager.hpp>

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  #include <hicr/backends/mpi/L1/communicationManager.hpp>
  #include <hicr/backends/mpi/L1/instanceManager.hpp>
  #include <hicr/backends/mpi/L1/memoryManager.hpp>
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE
  #include <hicr/backends/pthreads/L1/communicationManager.hpp>
  #include <hicr/backends/hwloc/L1/instanceManager.hpp>
  #include <hicr/backends/hwloc/L1/memoryManager.hpp>
#endif

#include "grid.hpp"
#include "task.hpp"

int main(int argc, char *argv[])
{
  //// Instantiating distributed execution machinery

  // Storage for the distributed engine's communication manager
  std::unique_ptr<HiCR::L1::CommunicationManager> communicationManager;

  // Storage for the distributed engine's instance manager
  std::unique_ptr<HiCR::L1::InstanceManager> instanceManager;

  // Storage for the distributed engine's memory manager
  std::unique_ptr<HiCR::L1::MemoryManager> memoryManager;

#ifdef _TASKR_DISTRIBUTED_ENGINE_LPF
  #error "LPF backend not supported yet"
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  instanceManager      = HiCR::backend::mpi::L1::InstanceManager::createDefault(&argc, &argv);
  communicationManager = std::make_unique<HiCR::backend::mpi::L1::CommunicationManager>();
  memoryManager        = std::make_unique<HiCR::backend::mpi::L1::MemoryManager>();
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE
  instanceManager      = std::make_unique<HiCR::backend::hwloc::L1::InstanceManager>();
  communicationManager = std::make_unique<HiCR::backend::pthreads::L1::CommunicationManager>();
  memoryManager        = HiCR::backend::hwloc::L1::MemoryManager::createDefault();
#endif

  // Creating (local host) topology manager
  const auto topologyManager = HiCR::backend::hwloc::L1::TopologyManager::createDefault();

  // Getting distributed instance information
  const auto instanceCount  = instanceManager->getInstances().size();
  const auto myInstanceId   = instanceManager->getCurrentInstance()->getId();
  const auto rootInstanceId = instanceManager->getRootInstanceId();
  const auto isRootInstance = myInstanceId == rootInstanceId;

  //// Setting up Taskr

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();
  //printf("Topology: %s\n", t.serialize().dump(2).c_str());

  // Getting NUMA Domain information
  const auto &numaDomains = t.getDevices();
  printf("NUMA Domains per Node: %lu\n", numaDomains.size());

  // Assuming one process per numa domain
  size_t numaDomainId = myInstanceId % numaDomains.size();
  auto   numaDomain   = numaDomains[numaDomainId];
  printf("Instance %lu - Using NUMA domain: %lu\n", myInstanceId, numaDomainId);

  // Updating the compute resource list
  auto computeResources = numaDomain->getComputeResourceList();
  printf("PUs Per NUMA Domain: %lu\n", computeResources.size());

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::L1::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::L1::ComputeManager pthreadsComputeManager;

  // Creating taskr object
  nlohmann::json taskrConfig;
  taskrConfig["Remember Finished Objects"] = true;
  taskr::Runtime taskr(&boostComputeManager, &pthreadsComputeManager, computeResources, taskrConfig);

  // Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation is finished
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&taskr](taskr::Task *task) { taskr.resumeTask(task); });

  //// Setting up application configuration

  // Setting default values
  size_t  gDepth = 1;
  size_t  N      = 128;
  ssize_t nIters = 100;
  D3      pt     = D3({.x = 1, .y = 1, .z = 1});
  D3      lt     = D3({.x = 1, .y = 1, .z = 1});

  // Parsing user inputs
  for (int i = 0; i < argc; i++)
  {
    if (!strcmp(argv[i], "-px")) pt.x = atoi(argv[++i]);
    if (!strcmp(argv[i], "-py")) pt.y = atoi(argv[++i]);
    if (!strcmp(argv[i], "-pz")) pt.z = atoi(argv[++i]);
    if (!strcmp(argv[i], "-lx")) lt.x = atoi(argv[++i]);
    if (!strcmp(argv[i], "-ly")) lt.y = atoi(argv[++i]);
    if (!strcmp(argv[i], "-lz")) lt.z = atoi(argv[++i]);
    if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
    if (!strcmp(argv[i], "-i")) nIters = atoi(argv[++i]);
  }

  if ((size_t)(pt.x * pt.y * pt.z) != instanceCount)
  {
    if (isRootInstance) printf("[Error] The specified px/py/pz geometry does not match the number of instances (-n %lu).\n", instanceCount);
    instanceManager->abort(-1);
  }

  // Creating and initializing Grid
  auto g       = std::make_unique<Grid>(myInstanceId, N, nIters, gDepth, pt, lt, &taskr, memoryManager.get(), topologyManager.get(), communicationManager.get());
  bool success = g->initialize();
  if (success == false) instanceManager->abort(-1);

  // Creating grid processing functions
  g->resetFc = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->reset(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k); });
  g->computeFc =
    std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->compute(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->receiveFc =
    std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->receive(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->unpackFc = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->unpack(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->packFc   = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->pack(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->sendFc   = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->send(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->localResidualFc = std::make_unique<taskr::Function>(
    [&g](taskr::Task *task) { g->calculateLocalResidual(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });

  // Task map
  std::map<taskr::label_t, std::shared_ptr<taskr::Task>> _taskMap;

  // printf("Instance %lu: Executing...\n", myInstanceId);

  // Creating tasks to reset the grid
  for (ssize_t i = 0; i < lt.x; i++)
    for (ssize_t j = 0; j < lt.y; j++)
      for (ssize_t k = 0; k < lt.z; k++)
      {
        auto resetTask = new Task("Reset", i, j, k, 0, g->resetFc.get());
        taskr.addTask(resetTask);
      }

  // Initializing TaskR
  taskr.initialize();

  // Running Taskr initially
  taskr.run();

  // Waiting for Taskr to finish
  taskr.await();

  // Creating and adding tasks (graph nodes)
  for (ssize_t it = 0; it < nIters; it++)
    for (ssize_t i = 0; i < lt.x; i++)
      for (ssize_t j = 0; j < lt.y; j++)
        for (ssize_t k = 0; k < lt.z; k++)
        {
          auto  localId = g->localSubGridMapping[k][j][i];
          auto &subGrid = g->subgrids[localId];

          // create new specific tasks
          auto computeTask = std::make_shared<Task>("Compute", i, j, k, it, g->computeFc.get());
          auto packTask    = std::make_shared<Task>("Pack", i, j, k, it, g->packFc.get());
          auto sendTask    = std::make_shared<Task>("Send", i, j, k, it, g->sendFc.get());
          auto recvTask    = std::make_shared<Task>("Receive", i, j, k, it, g->receiveFc.get());
          auto unpackTask  = std::make_shared<Task>("Unpack", i, j, k, it, g->unpackFc.get());

          _taskMap[Task::encodeTaskName("Compute", i, j, k, it)] = computeTask;
          _taskMap[Task::encodeTaskName("Pack", i, j, k, it)]    = packTask;
          _taskMap[Task::encodeTaskName("Send", i, j, k, it)]    = sendTask;
          _taskMap[Task::encodeTaskName("Receive", i, j, k, it)] = recvTask;
          _taskMap[Task::encodeTaskName("Unpack", i, j, k, it)]  = unpackTask;

          // Creating and adding local compute task dependencies
          if (it > 0)
            if (subGrid.X0.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i - 1, j + 0, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.X1.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 1, j + 0, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.Y0.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j - 1, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.Y1.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 1, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.Z0.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 0, k - 1, it - 1)].get());
          if (it > 0)
            if (subGrid.Z1.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 0, k + 1, it - 1)].get());
          if (it > 0) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 0, k + 0, it - 1)].get());

          // Adding communication-related dependencies
          if (it > 0) computeTask->addDependency(_taskMap[Task::encodeTaskName("Pack", i, j, k, it - 1)].get());
          if (it > 0) computeTask->addDependency(_taskMap[Task::encodeTaskName("Unpack", i, j, k, it - 1)].get());

          // Creating and adding receive task dependencies, from iteration 1 onwards
          if (it > 0) recvTask->addDependency(_taskMap[Task::encodeTaskName("Unpack", i, j, k, it - 1)].get());

          // Creating and adding unpack task dependencies
          unpackTask->addDependency(_taskMap[Task::encodeTaskName("Receive", i, j, k, it)].get());
          unpackTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i, j, k, it)].get());

          // Creating and adding send task dependencies, from iteration 1 onwards
          packTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i, j, k, it)].get());
          if (it > 0) packTask->addDependency(_taskMap[Task::encodeTaskName("Send", i, j, k, it - 1)].get());

          // Creating and adding send task dependencies, from iteration 1 onwards
          sendTask->addDependency(_taskMap[Task::encodeTaskName("Pack", i, j, k, it)].get());

          // Adding tasks to taskr
          taskr.addTask(computeTask.get());
          if (it < nIters - 1) taskr.addTask(packTask.get());
          if (it < nIters - 1) taskr.addTask(sendTask.get());
          if (it < nIters - 1) taskr.addTask(recvTask.get());
          if (it < nIters - 1) taskr.addTask(unpackTask.get());
        }

  // Setting start time as now
  auto t0 = std::chrono::high_resolution_clock::now();

  // Running Taskr
  taskr.run();

  // Waiting for Taskr to finish
  taskr.await();

  ////// Calculating residual

  // Reset local residual to zero
  g->resetResidual();

  // Calculating local residual
  for (ssize_t i = 0; i < lt.x; i++)
    for (ssize_t j = 0; j < lt.y; j++)
      for (ssize_t k = 0; k < lt.z; k++)
      {
        auto residualTask = new Task("Residual", i, j, k, nIters, g->localResidualFc.get());
        taskr.addTask(residualTask);
      }

  // Running Taskr
  taskr.run();

  // Waiting for Taskr to finish
  taskr.await();

  // Finalizing TaskR
  taskr.finalize();

  // If i'm not the root instance, simply send my locally calculated reisdual
  if (isRootInstance == false)
  {
    *(double *)g->residualSendBuffer->getPointer() = g->_residual;
    g->residualProducerChannel->push(g->residualSendBuffer, 1);
  }
  else
  {
    // Otherwise gather all the residuals and print the results
    double globalRes = g->_residual;

    for (size_t i = 0; i < instanceCount - 1; i++)
    {
      while (g->residualConsumerChannel->isEmpty());
      double *residualPtr = (double *)g->residualConsumerChannel->getTokenBuffer()->getSourceLocalMemorySlot()->getPointer() + g->residualConsumerChannel->peek(0);
      g->residualConsumerChannel->pop();
      globalRes += *residualPtr;
    }

    // Setting final time now
    auto                         tf       = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> dt       = tf - t0;
    float                        execTime = dt.count();

    double residual = sqrt(globalRes / ((double)(N - 1) * (double)(N - 1) * (double)(N - 1)));
    double gflops   = nIters * (double)N * (double)N * (double)N * (2 + gDepth * 8) / (1.0e9);
    printf("%.4fs, %.3f GFlop/s (L2 Norm: %.10g)\n", execTime, gflops / execTime, residual);
  }

  // Finalizing grid
  g->finalize();

  // Finalizing instances
  instanceManager->finalize();
}