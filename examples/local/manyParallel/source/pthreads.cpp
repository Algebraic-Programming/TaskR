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

#include <hwloc.h>
#include <hicr/backends/pthreads/computeManager.hpp>
#include <hicr/backends/hwloc/topologyManager.hpp>
#include <hicr/backends/pthreads/computeManager.hpp>
#include <hicr/backends/boost/computeManager.hpp>
#include "manyParallel.hpp"

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "Wrong Usage. Syntax: ./manyParallel NBRANCHES NTASKS\n");
    exit(1);
  }

  // Getting arguments, if provided
  const size_t taskCount   = std::atoi(argv[1]);
  const size_t branchCount = std::atoi(argv[2]);

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based (CPU) topology manager
  HiCR::backend::hwloc::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::ComputeManager pthreadsComputeManager;

  // Creating taskr
  taskr::Runtime taskr(&boostComputeManager, &pthreadsComputeManager, computeResources);

  // Running manyParallel example
  manyParallel(taskr, branchCount, taskCount);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
