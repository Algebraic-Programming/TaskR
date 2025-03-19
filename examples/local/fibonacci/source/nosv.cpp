#include <cstdio>
#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>

#include <nosv.h>
#include <hicr/backends/nosv/common.hpp>
#include <hicr/backends/nosv/L1/computeManager.hpp>

#include "fibonacci.hpp"

int main(int argc, char **argv)
{
  // Checking arguments
  if (argc != 2)
  {
    fprintf(stderr, "Error: Must provide the fibonacci number to calculate.\n");
    exit(-1);
  }

  // Initialize nosv
  check(nosv_init());

  // nosv task instance for the main thread
  nosv_task_t mainTask;

  // Attaching the main thread
  check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

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

  // Getting compute resources in this device
  auto cr = (*(t.getDevices().begin()))->getComputeResourceList();

  // Adding it to the list
  auto itr = cr.begin();
  for (int i = 0; i < 8; i++)
  {
    computeResources.push_back(*itr);
    itr++;
  }

  // Initializing nosv-based compute manager to run tasks in parallel
  HiCR::backend::nosv::L1::ComputeManager computeManager;

  // Creating taskr
  taskr::Runtime taskr(&computeManager, &computeManager, computeResources);

  // Running Fibonacci example
  auto result = fibonacciDriver(initialValue, taskr);

  // Printing result
  printf("Fib(%lu) = %lu\n", initialValue, result);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  // Detaching the main thread
  check(nosv_detach(NOSV_DETACH_NONE));

  // Shutdown nosv
  check(nosv_shutdown());

  return 0;
}
