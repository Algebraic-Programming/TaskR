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
#include <hicr/backends/pthreads/computeManager.hpp>
#include <hicr/backends/hwloc/topologyManager.hpp>
#include <hicr/backends/boost/computeManager.hpp>
#include "fibonacci.hpp"

int main(int argc, char **argv)
{
  // Checking arguments
  if (argc != 2)
  {
    fprintf(stderr, "Error: Must provide the fibonacci number to calculate.\n");
    exit(-1);
  }

  // Reading argument
  uint64_t initialValue = std::atoi(argv[1]);

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::hwloc::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Compute resources to use
  HiCR::Device::computeResourceList_t computeResources;

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::ComputeManager pthreadsComputeManager;

  // Getting compute resources in this device
  auto cr = (*(t.getDevices().begin()))->getComputeResourceList();

  // Adding it to the list
  auto itr = cr.begin();
  auto numCores = std::max(8ul, cr.size());
  for (int i = 0; i < numCores; i++)
  {
    computeResources.push_back(*itr);
    itr++;
  }

  // Initializing Pthreads-based compute manager to run tasks in parallel
  HiCR::backend::pthreads::ComputeManager computeManager;

  // Instantiating TaskR
  taskr::Runtime taskr(&boostComputeManager, &pthreadsComputeManager, computeResources);

  // Running Fibonacci example
  auto result = fibonacciDriver(initialValue, taskr);
  
  // Printing result
  printf("Fib(%lu) = %lu\n", initialValue, result);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
