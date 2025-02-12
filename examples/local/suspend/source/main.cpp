#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include "suspend.hpp"

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "Wrong Usage. Syntax: ./suspend NBRANCHES NTASKS\n");
    exit(1);
  }

  // Getting arguments, if provided
  const size_t taskCount   = std::atoi(argv[1]);
  const size_t branchCount = std::atoi(argv[2]);

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Creating taskr
  taskr::Runtime taskr(computeResources);

  // Running suspend example
  suspend(taskr, branchCount, taskCount);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
