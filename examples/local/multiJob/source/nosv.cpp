#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>

#include <nosv.h>
#include <hicr/backends/nosv/common.hpp>
#include <hicr/backends/nosv/L1/computeManager.hpp>

#include "jobs.hpp"

int main(int argc, char **argv)
{
  // Initialize nosv
  check(nosv_init());

  // nosv task instance for the main thread
  nosv_task_t mainTask;

  // Attaching the main thread
  check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

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

  // Initializing nosv-based compute manager to run tasks in parallel
  HiCR::backend::nosv::L1::ComputeManager computeManager;

  // Creating taskr
  taskr::Runtime taskr(&computeManager, &computeManager, computeResources);

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

  // Detaching the main thread
  check(nosv_detach(NOSV_DETACH_NONE));

  // Shutdown nosv
  check(nosv_shutdown());

  return 0;
}
