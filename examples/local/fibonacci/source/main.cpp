#include <cstdio>
#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <hicr/backends/boost/L1/computeManager.hpp>
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
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Compute resources to use
  HiCR::L0::Device::computeResourceList_t computeResources;

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::L1::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::L1::ComputeManager pthreadsComputeManager;

  // Getting compute resources in this device
  auto cr = (*(t.getDevices().begin()))->getComputeResourceList();

  // Adding it to the list
  auto itr = cr.begin();
  for (int i = 0; i < 8; i++)
  {
    computeResources.push_back(*itr);
    itr++;
  }

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
