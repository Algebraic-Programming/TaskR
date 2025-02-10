#include <cstdio>
#include <hwloc.h>
#include <hicr/backends/hwloc/L1/topologyManager.hpp>
#include <taskr/taskr.hpp>

void workFc(const size_t iterations)
{
  __volatile__ double value = 2.0;
  for (size_t i = 0; i < iterations; i++)
    for (size_t j = 0; j < iterations; j++)
    {
      value = sqrt(value + i);
      value = value * value;
    }
}

void waitFc(taskr::Runtime *taskr, size_t secondsDelay)
{
  printf("Starting long task...\n");
  fflush(stdout);

  sleep(secondsDelay);

  printf("Finished long task...\n");
  fflush(stdout);
}

int main(int argc, char **argv)
{
  // Getting arguments, if provided
  size_t workTaskCount = 1000;
  size_t secondsDelay  = 5;
  size_t iterations    = 5000;
  if (argc > 1) workTaskCount = std::atoi(argv[1]);
  if (argc > 2) secondsDelay = std::atoi(argv[2]);
  if (argc > 3) iterations = std::atoi(argv[3]);

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing Pthreads-based compute manager to run tasks in parallel
  HiCR::backend::pthreads::L1::ComputeManager computeManager;

  // Initializing HWLoc-based (CPU) topology manager
  HiCR::backend::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  // Initializing taskr
  taskr::Runtime taskr(computeResources);

  // Setting onTaskFinish callback to free up task's memory upon finishing
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Creating task work function
  auto workFunction = taskr::Function([&iterations](taskr::Task *task) { workFc(iterations); });

  // Creating task wait function
  auto waitFunction = taskr::Function([&taskr, &secondsDelay](taskr::Task *task) { waitFc(&taskr, secondsDelay); });

  // Creating a single wait task that suspends all workers except for one
  auto waitTask1 = new taskr::Task(0, &waitFunction);

  // Building task graph. First a lot of pure work tasks. The wait task depends on these
  for (size_t i = 0; i < workTaskCount; i++)
  {
    auto workTask = new taskr::Task(i + 1, &workFunction);
    waitTask1->addDependency(workTask);
    taskr.addTask(workTask);
  }

  // Creating another wait task
  auto waitTask2 = new taskr::Task(2 * workTaskCount + 1, &waitFunction);

  // Then creating another batch of work tasks that depends on the wait task
  for (size_t i = 0; i < workTaskCount; i++)
  {
    auto workTask = new taskr::Task(workTaskCount + i + 1, &workFunction);

    // This work task waits on the first wait task
    workTask->addDependency(waitTask1);

    // The second wait task depends on this work task
    waitTask2->addDependency(workTask);

    // Adding work task
    taskr.addTask(workTask);
  }

  // Last set of work tasks
  for (size_t i = 0; i < workTaskCount; i++)
  {
    auto workTask = new taskr::Task(2 * workTaskCount + i + 2, &workFunction);

    // This work task depends on the second wait task
    workTask->addDependency(waitTask2);

    // Adding work task
    taskr.addTask(workTask);
  }

  // Adding work tasks
  taskr.addTask(waitTask1);
  taskr.addTask(waitTask2);

  // Initializing taskr
  taskr.initialize();

  // Running taskr
  printf("Starting (open 'htop' in another console to see the workers going to sleep during the long task)...\n");
  taskr.run();
  taskr.await();
  printf("Finished.\n");

  // Finalizing taskr
  taskr.finalize();

  // Freeing up memory
  hwloc_topology_destroy(topology);

  return 0;
}
