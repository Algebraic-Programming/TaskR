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
#include <hwloc.h>
#include <hicr/backends/host/pthreads/L1/communicationManager.hpp>
#include <hicr/backends/host/hwloc/L1/memoryManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>

#include "cholesky.hpp"

int main(int argc, char **argv)
{
  // Checking arguments
  if (argc != 5)
  {
    fprintf(stderr, "Error: <matrix size> <blocks> <check result> <matrix path>\n");
    exit(-1);
  }

  // Reading argument
  uint32_t    matrixSize  = std::atoi(argv[1]);
  uint32_t    blocks      = std::atoi(argv[2]);
  bool        checkResult = std::atoi(argv[3]);
  std::string matrixPath  = std::string(argv[4]);

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based host (CPU) topology and memory manager
  HiCR::backend::host::hwloc::L1::TopologyManager         tm(&topology);
  HiCR::backend::host::hwloc::L1::MemoryManager           memoryManager(&topology);
  HiCR::backend::host::pthreads::L1::CommunicationManager communicationManager;

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Memory space to use
  auto memorySpace = (*(*t.getDevices().begin())->getMemorySpaceList().begin());

  // Running Cholesky factorization example
  choleskyDriver(matrixSize, blocks, checkResult, &memoryManager, &communicationManager, memorySpace, matrixPath);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
