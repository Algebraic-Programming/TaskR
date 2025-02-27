#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <hicr/backends/pthreads/L1/computeManager.hpp>
#include <hicr/backends/boost/L1/computeManager.hpp>
#include "jobs.hpp"

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

  // Setting onTaskFinish callback to free up the task's memory on finish
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Adding multiple jobs to TaskR
  job1(taskr);
  job2(taskr);

  // Initializing taskR
  taskr.initialize();

  // Running taskr
  taskr.run();

  // Waiting for taskr to finish
  taskr.await();

  // Finalizing taskR
  taskr.finalize();

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
