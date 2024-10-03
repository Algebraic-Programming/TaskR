#include <cstdio>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
#include <taskr/taskr.hpp>

#define TASK_LABEL 42

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

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Instantiating taskr
  taskr::Runtime taskr(computeResources);

  // Creating task  execution unit
  auto taskFc = taskr::Function([&](taskr::Task* task) {
    // Printing associated task label
    const auto myTask = taskr::getCurrentTask();
    printf("Current Task Pointer: %p and Label: %lu.\n", myTask, myTask->getLabel());
  });

  // Creating a single task to print the internal references
  taskr::Task task(TASK_LABEL, &taskFc);

  // Adding task to TaskR
  taskr.addTask(&task);

  // Initializing taskr
  taskr.initialize();

  // Running taskr
  taskr.run();

  // Waiting for taskr to finish
  taskr.await();

  // Finalizing taskr
  taskr.finalize();

  return 0;
}
