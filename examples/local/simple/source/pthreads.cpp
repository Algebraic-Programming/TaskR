#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <hicr/backends/boost/L1/computeManager.hpp>
#include <hicr/backends/pthreads/L1/computeManager.hpp>

#include "simple.hpp"

int main(int argc, char **argv)
{
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
  for (int i = 0; i < 1; i++)
  {
    computeResources.push_back(*itr);
    itr++;
  }

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::L1::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::L1::ComputeManager pthreadsComputeManager;

  // Creating taskr
  taskr::Runtime taskr(&boostComputeManager, &pthreadsComputeManager, computeResources);

  // Running ABCtasks example
  simple(&taskr);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
