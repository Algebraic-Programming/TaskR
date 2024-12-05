#include <cstdio>
#include <random>
#include <hwloc.h>
#include <chrono>
#include <pthread.h>
#include <hicr/backends/host/pthreads/L1/communicationManager.hpp>
#include <hicr/backends/host/hwloc/L1/memoryManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>

#include "cholesky.hpp"

// Global variables
taskr::Runtime                                              *_taskr;
HiCR::backend::host::L1::ComputeManager                     *_computeManager;
std::atomic<uint64_t>                                       *_taskCounter;
std::vector<std::vector<std::unordered_set<taskr::Task*>>> _dependencyGrid;

int main(int argc, char **argv)
{
  // Checking arguments
  if (argc < 5)
  {
    fprintf(stderr, "Error: <matrix size> <blocks> <check result> <matrix path>\n");
    exit(-1);
  }

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

  // Initializing HWLoc-based host (CPU) topology and memory manager
  HiCR::backend::host::hwloc::L1::TopologyManager tm(&topology);
  HiCR::backend::host::hwloc::L1::MemoryManager   memoryManager(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Memory space to use
  auto memorySpace = (*(*t.getDevices().begin())->getMemorySpaceList().begin());

  // Adding compute resources of a single NUMA domain
  auto computeResources = (*t.getDevices().begin())->getComputeResourceList();

  // Initializing Pthreads-based compute manager to run tasks in parallel
  HiCR::backend::host::pthreads::L1::ComputeManager       computeManager;
  HiCR::backend::host::pthreads::L1::CommunicationManager communicationManager;

  // Running Cholesky factorization example
  choleskyDriver(matrixDimension, blocks, readFromFile, checkResult, &memoryManager, &communicationManager, computeResources, memorySpace, matrixPath);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
