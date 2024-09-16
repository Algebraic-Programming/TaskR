#include <cstdio>
#include <hicr/backends/host/pthreads/L1/computeManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
#include <taskr/taskr.hpp>

#define TASK_LABEL 42

int main(int argc, char **argv)
{
  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing Pthread-based compute manager to run tasks in parallel
  HiCR::backend::host::pthreads::L1::ComputeManager computeManager;

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::host::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Instantiating taskr
  taskr::Runtime taskr(&computeManager);

  // Create processing units from the detected compute resource list and giving them to taskr
  for (auto resource : computeResources)
  {
    // Creating a processing unit out of the computational resource
    auto processingUnit = computeManager.createProcessingUnit(resource);

    // Assigning resource to the taskr
    taskr.addProcessingUnit(std::move(processingUnit));
  }

  // Creating task  execution unit
  auto taskExecutionUnit = computeManager.createExecutionUnit([&taskr]() {
    // Printing associated task label
    const auto myTask = taskr::getCurrentTask();
    printf("Current Task Pointer: %p and Label: %lu.\n", myTask, myTask->getLabel());
  });

  // Creating a single task to print the internal references
  taskr::Task task(TASK_LABEL, taskExecutionUnit);

  // Adding task to TaskR
  taskr.addTask(&task);

  // Initializing taskr
  taskr.initialize();
  
  // Running taskr
  taskr.run();

  // Finalizing taskr
  taskr.finalize();

  return 0;
}
