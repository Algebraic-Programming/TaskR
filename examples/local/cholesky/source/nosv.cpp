#include <cstdio>
#include <random>
#include <hwloc.h>
#include <chrono>
#include <pthread.h>
#include <hicr/backends/pthreads/L1/communicationManager.hpp>
#include <hicr/backends/hwloc/L1/memoryManager.hpp>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>

#include <nosv.h>
#include <hicr/backends/nosv/common.hpp>
#include <hicr/backends/nosv/L1/computeManager.hpp>

#include "cholesky.hpp"

// Global variables
taskr::Runtime                                             *_taskr;
HiCR::backend::pthreads::L1::ComputeManager                *_computeManager;
std::atomic<uint64_t>                                      *_taskCounter;
std::vector<std::vector<std::unordered_set<taskr::Task *>>> _dependencyGrid;

int main(int argc, char **argv)
{
  // Checking arguments
  if (argc < 5)
  {
    fprintf(stderr, "Error: <matrix size> <blocks> <check result> <matrix path>\n");
    exit(-1);
  }

  // Initialize nosv
  check(nosv_init());

  // nosv task instance for the main thread
  nosv_task_t mainTask;

  // Attaching the main threadstinktSau
  check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

  // Reading argument
  uint32_t    matrixDimension = std::atoi(argv[1]);
  uint32_t    blocks          = std::atoi(argv[2]);
  bool        readFromFile    = std::atoi(argv[3]);
  bool        checkResult     = std::atoi(argv[4]);
  std::string matrixPath      = "";
  if (argc == 6) { matrixPath = std::string(argv[5]); }

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based (CPU) topology and memory manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);
  HiCR::backend::hwloc::L1::MemoryManager   memoryManager(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Memory space to use
  auto memorySpace = (*(*t.getDevices().begin())->getMemorySpaceList().begin());

  // Adding compute resources of a single NUMA domain
  auto computeResources = (*t.getDevices().begin())->getComputeResourceList();

  // Initializing Pthreads-based compute manager to run tasks in parallel
  HiCR::backend::nosv::L1::ComputeManager           computeManager;
  HiCR::backend::pthreads::L1::CommunicationManager communicationManager;

  // Creating taskr object
  nlohmann::json taskrConfig;
  taskrConfig["Remember Finished Objects"] = true;
  taskr::Runtime taskr(&computeManager, &computeManager, computeResources, taskrConfig);

  // Running Cholesky factorization example
  choleskyDriver(taskr, matrixDimension, blocks, readFromFile, checkResult, &memoryManager, &communicationManager, memorySpace, matrixPath);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  // Detaching the main thread
  check(nosv_detach(NOSV_DETACH_NONE));

  // Shutdown nosv
  check(nosv_shutdown());

  return 0;
}
