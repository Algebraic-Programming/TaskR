#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <hicr/backends/pthreads/L1/computeManager.hpp>
#include <hicr/backends/boost/L1/computeManager.hpp>
#include "mutex.hpp"

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

  // Initializing Boost-based compute manager to instantiate suspendable coroutines
  HiCR::backend::boost::L1::ComputeManager boostComputeManager;

  // Initializing Pthreads-based compute manager to instantiate processing units
  HiCR::backend::pthreads::L1::ComputeManager pthreadsComputeManager;

  // Creating taskr
  taskr::Runtime taskr(&boostComputeManager, &pthreadsComputeManager, computeResources);

  // Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation (required by mutex) is finished
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&taskr](taskr::Task *task) { taskr.resumeTask(task); });

  // Running ABCtasks example
  mutex(&taskr);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
