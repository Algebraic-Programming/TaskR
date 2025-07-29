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
#include <hicr/core/communicationManager.hpp>
#include <hicr/core/instanceManager.hpp>
#include <hicr/core/memoryManager.hpp>
#include <hicr/backends/hwloc/topologyManager.hpp>

#include <nosv.h>
#include <hicr/backends/nosv/common.hpp>
#include <hicr/backends/nosv/computeManager.hpp>

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  #include <hicr/backends/mpi/communicationManager.hpp>
  #include <hicr/backends/mpi/instanceManager.hpp>
  #include <hicr/backends/mpi/memoryManager.hpp>
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE
  #include <hicr/backends/pthreads/communicationManager.hpp>
  #include <hicr/backends/hwloc/instanceManager.hpp>
  #include <hicr/backends/hwloc/memoryManager.hpp>
#endif

#include "grid.hpp"
#include "task.hpp"
#include "jacobi3d.hpp"

// Setting default values (globali)
size_t  gDepth = 1;
size_t  N      = 128;
ssize_t nIters = 100;
D3      pt     = D3({.x = 1, .y = 1, .z = 1});
D3      lt     = D3({.x = 1, .y = 1, .z = 1});

void jacobiDriver(HiCR::InstanceManager *instanceManager, HiCR::CommunicationManager *communicationManager, HiCR::MemoryManager *memoryManager)
{
  // Creating (local host) topology manager
  const auto topologyManager = HiCR::backend::hwloc::TopologyManager::createDefault();

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
  HiCR::backend::hwloc::TopologyManager tm(&topology);

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

  // Initializing nosv-based compute manager to run tasks in parallel
  HiCR::backend::nosv::ComputeManager computeManager;

  // Creating taskr object
  nlohmann::json taskrConfig;
  taskrConfig["Remember Finished Objects"] = true;
  taskr::Runtime taskr(&computeManager, &computeManager, computeResources, taskrConfig);

  // Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation is finished
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&taskr](taskr::Task *task) { taskr.resumeTask(task); });

  if ((size_t)(pt.x * pt.y * pt.z) != instanceCount)
  {
    if (isRootInstance) printf("[Error] The specified px/py/pz geometry does not match the number of instances (-n %lu).\n", instanceCount);
    instanceManager->abort(-1);
  }

  // Creating and initializing Grid
  auto g = std::make_unique<Grid>(myInstanceId, N, nIters, gDepth, pt, lt, &taskr, memoryManager, topologyManager.get(), communicationManager);

  // running the Jacobi3D example
  jacobi3d(instanceManager, taskr, g.get(), gDepth, N, nIters, pt, lt);

  // Finalizing instances
  instanceManager->finalize();
}

int main(int argc, char *argv[])
{
  // Initialize nosv
  check(nosv_init());

  // nosv task instance for the main thread
  nosv_task_t mainTask;

  // Attaching the main thread
  check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

  //// Instantiating distributed execution machinery

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

#ifdef _TASKR_DISTRIBUTED_ENGINE_LPF
  #error "LPF backend not supported yet for nOS-V backend"
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  std::unique_ptr<HiCR::InstanceManager>      instanceManager      = HiCR::backend::mpi::InstanceManager::createDefault(&argc, &argv);
  std::unique_ptr<HiCR::CommunicationManager> communicationManager = std::make_unique<HiCR::backend::mpi::CommunicationManager>();
  std::unique_ptr<HiCR::MemoryManager>        memoryManager        = std::make_unique<HiCR::backend::mpi::MemoryManager>();

  // Running the remote memcpy example
  jacobiDriver(instanceManager.get(), communicationManager.get(), memoryManager.get());
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE // This one segfaults (check why)
  std::unique_ptr<HiCR::InstanceManager>      instanceManager      = std::make_unique<HiCR::backend::hwloc::InstanceManager>();
  std::unique_ptr<HiCR::CommunicationManager> communicationManager = std::make_unique<HiCR::backend::pthreads::CommunicationManager>();
  std::unique_ptr<HiCR::MemoryManager>        memoryManager        = HiCR::backend::hwloc::MemoryManager::createDefault();

  // Running the remote memcpy example
  jacobiDriver(instanceManager.get(), communicationManager.get(), &memoryManager.get());
#endif

  // Detaching the main thread
  check(nosv_detach(NOSV_DETACH_NONE));

  // Shutdown nosv
  check(nosv_shutdown());
}