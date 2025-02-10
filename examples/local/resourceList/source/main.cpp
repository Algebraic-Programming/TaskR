#include <chrono>
#include <cstdio>
#include <hwloc.h>
#include <hicr/backends/pthreads/L1/computeManager.hpp>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <taskr/taskr.hpp>
#include "source/workTask.hpp"

int main(int argc, char **argv)
{
  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing Pthread-base compute manager to run tasks in parallel
  HiCR::backend::pthreads::L1::ComputeManager computeManager;

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting compute resource lists from devices
  std::vector<HiCR::L0::Device::computeResourceList_t> computeResourceLists;
  for (auto d : t.getDevices()) computeResourceLists.push_back(d->getComputeResourceList());

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
  HiCR::L0::Device::computeResourceList_t selectedComputeResources;
  for (auto computeResourceList : computeResourceLists)
    for (auto computeResource : computeResourceList)
    {
      // Interpreting compute resource as core
      auto core = dynamic_pointer_cast<HiCR::backend::hwloc::L0::ComputeResource>(computeResource);

      // If the core affinity is included in the list, Add it to the list
      if (coreSubset.contains(core->getProcessorId())) selectedComputeResources.push_back(computeResource);
    }

  // Creating taskr
  taskr::Runtime taskr(selectedComputeResources);

  // Creating task function
  auto taskFunction = taskr::Function([&iterations](taskr::Task *task) { work(iterations); });

  // Adding multiple compute tasks
  printf("Running %lu work tasks with %lu processing units...\n", workTaskCount, coreSubset.size());
  for (size_t i = 0; i < workTaskCount; i++) taskr.addTask(new taskr::Task(i, &taskFunction));

  // Initializing taskR
  taskr.initialize();

  // Running taskr only on the core subset
  auto t0 = std::chrono::high_resolution_clock::now();
  taskr.run();
  taskr.await();
  auto tf = std::chrono::high_resolution_clock::now();

  auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(tf - t0).count();
  printf("Finished in %.3f seconds.\n", (double)dt * 1.0e-9);

  // Finalizing taskR
  taskr.finalize();

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
