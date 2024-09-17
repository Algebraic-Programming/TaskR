#include <chrono>
#include <cstdio>
#include <hwloc.h>
#include <hicr/backends/host/pthreads/L1/computeManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
#include <taskr/taskr.hpp>
#include "source/workTask.hpp"

int main(int argc, char **argv)
{
  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing Pthread-base compute manager to run tasks in parallel
  HiCR::backend::host::pthreads::L1::ComputeManager computeManager;

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::host::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting compute resource lists from devices
  std::vector<HiCR::L0::Device::computeResourceList_t> computeResourceLists;
  for (auto d : t.getDevices()) computeResourceLists.push_back(d->getComputeResourceList());

  // Creating taskr
  taskr::Runtime taskr(&computeManager);

  // Getting work task count
  size_t workTaskCount = 100;
  size_t iterations    = 5000;
  if (argc > 1) workTaskCount = std::atoi(argv[1]);
  if (argc > 2) iterations = std::atoi(argv[2]);

  // Getting the core subset from the argument list (could be from a file too)
  std::set<int> coreSubset;
  for (int i = 3; i < argc; i++) coreSubset.insert(std::atoi(argv[i]));

  // Sanity check
  if (coreSubset.empty())
  {
    fprintf(stderr, "Launch error: no compute resources provided\n");
    exit(-1);
  }

  // Create processing units from the detected compute resource list and giving them to taskr
  for (auto computeResourceList : computeResourceLists)
    for (auto computeResource : computeResourceList)
    {
      // Interpreting compute resource as core
      auto core = dynamic_pointer_cast<HiCR::backend::host::L0::ComputeResource>(computeResource);

      // If the core affinity is included in the list, create new processing unit
      if (coreSubset.contains(core->getProcessorId()))
      {
        // Creating a processing unit out of the computational resource
        auto processingUnit = computeManager.createProcessingUnit(computeResource);

        // Assigning resource to the taskr
        taskr.addProcessingUnit(std::move(processingUnit));
      }
    }

  // Creating task  execution unit
  auto taskExecutionUnit = computeManager.createExecutionUnit([&iterations]() { work(iterations); });

  // Adding multiple compute tasks
  printf("Running %lu work tasks with %lu processing units...\n", workTaskCount, coreSubset.size());
  for (size_t i = 0; i < workTaskCount; i++) taskr.addTask(new taskr::Task(i, taskExecutionUnit));

  // Initializing taskR
  taskr.initialize();

  // Running taskr only on the core subset
  auto t0 = std::chrono::high_resolution_clock::now();
  taskr.run();
  auto tf = std::chrono::high_resolution_clock::now();

  auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(tf - t0).count();
  printf("Finished in %.3f seconds.\n", (double)dt * 1.0e-9);

  // Finalizing taskR
  taskr.finalize();

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
