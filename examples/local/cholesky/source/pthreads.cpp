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

#include <cstdio>
#include <random>
#include <hwloc.h>
#include <chrono>
#include <pthread.h>
#include <hicr/backends/pthreads/communicationManager.hpp>
#include <hicr/backends/pthreads/computeManager.hpp>
#include <hicr/backends/boost/computeManager.hpp>
#include <hicr/backends/hwloc/memoryManager.hpp>
#include <hicr/backends/hwloc/topologyManager.hpp>
#include <hicr/backends/pthreads/computeManager.hpp>

#include "cholesky.hpp"

// Global variables
taskr::Runtime                                             *_taskr;
HiCR::backend::pthreads::ComputeManager                *_computeManager;
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
  HiCR::backend::hwloc::TopologyManager tm(&topology);
  HiCR::backend::hwloc::MemoryManager   memoryManager(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Memory space to use
  auto memorySpace = (*(*t.getDevices().begin())->getMemorySpaceList().begin());

  // Adding compute resources of a single NUMA domain
  auto computeResources = (*t.getDevices().begin())->getComputeResourceList();

  // Initializing communication manager to handle data motion
  HiCR::backend::pthreads::CommunicationManager communicationManager;

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::ComputeManager pthreadsComputeManager;

  // Creating taskr object
  nlohmann::json taskrConfig;
  taskrConfig["Remember Finished Objects"] = true;

  // Creating taskr
  taskr::Runtime taskr(&boostComputeManager, &pthreadsComputeManager, computeResources, taskrConfig);

  // Running Cholesky factorization example
  choleskyDriver(taskr, matrixDimension, blocks, readFromFile, checkResult, &memoryManager, &communicationManager, memorySpace, matrixPath);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
