#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include "pendingOperation.hpp"

int main(int argc, char **argv)
{
  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based (CPU) topology manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Instantiating taskr
  taskr::Runtime taskr(computeResources);

  // Running ABCtasks example
  pendingOperation(taskr);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
