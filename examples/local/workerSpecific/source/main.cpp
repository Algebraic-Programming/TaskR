#include <hwloc.h>
#include <hicr/backends/host/pthreads/L1/computeManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
#include "workerSpecific.hpp"

int main(int argc, char **argv)
{
  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::host::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first NUMA domain found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Initializing Pthreads-based compute manager to run tasks in parallel
  HiCR::backend::host::pthreads::L1::ComputeManager computeManager;

  // Creating taskr
  taskr::Runtime taskr(&computeManager);

  // Assigning processing resources to TaskR
  for (const auto &computeResource : computeResources) taskr.addProcessingUnit(computeManager.createProcessingUnit(computeResource));

  // Running worker-specific example
  workerSpecific(taskr, computeResources.size());

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
