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

#ifdef _TASKR_DISTRIBUTED_ENGINE_LPF
  #include <lpf/core.h>
  #include <lpf/mpi.h>
  #include <mpi.h>
  #include <hicr/backends/lpf/communicationManager.hpp>
  #include <hicr/backends/mpi/instanceManager.hpp>
  #include <hicr/backends/lpf/memoryManager.hpp>
#endif

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
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Initialize nosv
  check(nosv_init());

  // nosv task instance for the main thread
  nosv_task_t mainTask;

  // Attaching the main thread
  check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

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
  // Looking for Domains that are not zero (Slurm non --exclusive issue)
  size_t numaDomainId;
  for (size_t i = 0; i < numaDomains.size(); ++i)
  {
    numaDomainId = (myInstanceId + i) % numaDomains.size();
    if (numaDomains[numaDomainId]->getComputeResourceList().size() > 0) { break; }
  }

  auto numaDomain = numaDomains[numaDomainId];
  printf("Instance %lu - Using NUMA domain: %lu\n", myInstanceId, numaDomainId);

  // Updating the compute resource list
  auto computeResources = numaDomain->getComputeResourceList();

  // Compute resources to use
  HiCR::Device::computeResourceList_t cr;

  for (int i = 0; i < size; ++i)
  {
    if (myInstanceId == (size_t)i)
    {
      auto itr = computeResources.begin();
      for (size_t i = 0; i < computeResources.size(); i++)
      {
        // Getting up-casted pointer for the processing unit
        auto c = dynamic_pointer_cast<HiCR::backend::hwloc::ComputeResource>(*itr);

        // Checking whether the execution unit passed is compatible with this backend
        if (c == nullptr) HICR_THROW_LOGIC("The passed compute resource is not supported by this processing unit type\n");

        // Getting the logical processor ID of the compute resource
        auto pid = c->getProcessorId();

        printf("%u ", pid);
        fflush(stdout);

        cr.push_back(*itr);

        itr++;
      }
      printf("]\n");
      fflush(stdout);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  // Initializing nosv-based compute manager to run tasks in parallel
  HiCR::backend::nosv::ComputeManager computeManager;

  // Creating taskr object
  nlohmann::json taskrConfig;
  taskrConfig["Remember Finished Objects"] = true;
  taskr::Runtime taskr(&computeManager, &computeManager, cr, taskrConfig);

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

  // Detaching the main thread
  check(nosv_detach(NOSV_DETACH_NONE));

  // Shutdown nosv
  // check(nosv_shutdown());
}

#ifdef _TASKR_DISTRIBUTED_ENGINE_LPF

// flag needed when using MPI to launch
const int LPF_MPI_AUTO_INITIALIZE = 0;

  /**
 * #DEFAULT_MEMSLOTS The memory slots used by LPF
 * in lpf_resize_memory_register . This value is currently
 * guessed as sufficiently large for a program
 */
  #define DEFAULT_MEMSLOTS 10000

  /**
 * #DEFAULT_MSGSLOTS The message slots used by LPF
 * in lpf_resize_message_queue . This value is currently
 * guessed as sufficiently large for a program
 */
  #define DEFAULT_MSGSLOTS 10000

// Global pointer to the
HiCR::InstanceManager *instanceManager;

void spmd(lpf_t lpf, lpf_pid_t pid, lpf_pid_t nprocs, lpf_args_t args)
{
  // Initializing LPF
  CHECK(lpf_resize_message_queue(lpf, DEFAULT_MSGSLOTS));
  CHECK(lpf_resize_memory_register(lpf, DEFAULT_MEMSLOTS));
  CHECK(lpf_sync(lpf, LPF_SYNC_DEFAULT));

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing host (CPU) topology manager
  HiCR::backend::hwloc::TopologyManager tm(&topology);

  // Creating memory and communication managers
  std::unique_ptr<HiCR::CommunicationManager> communicationManager = std::make_unique<HiCR::backend::lpf::CommunicationManager>(nprocs, pid, lpf);
  std::unique_ptr<HiCR::MemoryManager>        memoryManager        = std::make_unique<HiCR::backend::lpf::MemoryManager>(lpf);

  // Running the remote memcpy example
  jacobiDriver(instanceManager, communicationManager.get(), memoryManager.get());
}
#endif

int main(int argc, char *argv[])
{
  //// Instantiating distributed execution machinery

#ifdef _TASKR_DISTRIBUTED_ENGINE_LPF
  // Initializing instance manager
  auto im         = HiCR::backend::mpi::InstanceManager::createDefault(&argc, &argv);
  instanceManager = im.get();

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

  lpf_init_t init;
  lpf_args_t args;

  CHECK(lpf_mpi_initialize_with_mpicomm(MPI_COMM_WORLD, &init));
  CHECK(lpf_hook(init, &spmd, args));
  CHECK(lpf_mpi_finalize(init));
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  std::unique_ptr<HiCR::InstanceManager>      instanceManager      = HiCR::backend::mpi::InstanceManager::createDefault(&argc, &argv);
  std::unique_ptr<HiCR::CommunicationManager> communicationManager = std::make_unique<HiCR::backend::mpi::CommunicationManager>();
  std::unique_ptr<HiCR::MemoryManager>        memoryManager        = std::make_unique<HiCR::backend::mpi::MemoryManager>();

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

  // Running the remote memcpy example
  jacobiDriver(instanceManager.get(), communicationManager.get(), memoryManager.get());
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE // This one segfaults (check why)
  std::unique_ptr<HiCR::InstanceManager>      instanceManager      = std::make_unique<HiCR::backend::hwloc::InstanceManager>();
  std::unique_ptr<HiCR::CommunicationManager> communicationManager = std::make_unique<HiCR::backend::pthreads::CommunicationManager>();
  std::unique_ptr<HiCR::MemoryManager>        memoryManager        = HiCR::backend::hwloc::MemoryManager::createDefault();

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

  // Running the remote memcpy example
  jacobiDriver(instanceManager.get(), communicationManager.get(), &memoryManager.get());
#endif
}