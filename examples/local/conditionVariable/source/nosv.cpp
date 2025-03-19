#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>

#include <nosv.h>
#include <hicr/backends/nosv/common.hpp>
#include <hicr/backends/nosv/L1/computeManager.hpp>

#include "conditionVariableWait.hpp"
#include "conditionVariableWaitFor.hpp"
#include "conditionVariableWaitCondition.hpp"
#include "conditionVariableWaitForCondition.hpp"

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

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Initializing nosv-based compute manager to run tasks in parallel
  HiCR::backend::nosv::L1::ComputeManager computeManager;

  // Instantiating TaskR
  taskr::Runtime taskr(&computeManager, &computeManager, computeResources);

  // Allowing tasks to immediately resume upon suspension -- they won't execute until their pending operation (required by condition variable) is finished
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&taskr](taskr::Task *task) { taskr.resumeTask(task); });

  // Running test
  __TEST_FUNCTION_(taskr);

  // Freeing up memory
  hwloc_topology_destroy(topology);

  // Detaching the main thread
  check(nosv_detach(NOSV_DETACH_NONE));

  // Shutdown nosv
  check(nosv_shutdown());

  return 0;
}
