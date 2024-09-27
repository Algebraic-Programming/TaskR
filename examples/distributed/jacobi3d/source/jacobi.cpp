#include <chrono>
#include <hwloc.h>
#include <taskr/taskr.hpp>
#include <hicr/core/L1/communicationManager.hpp>
#include <hicr/core/L1/instanceManager.hpp>
#include <hicr/core/L1/memoryManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
#include <hicr/backends/host/pthreads/L1/computeManager.hpp>
#include <hicr/frontends/deployer/deployer.hpp>

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  #include <hicr/backends/mpi/L1/communicationManager.hpp>
  #include <hicr/backends/mpi/L1/instanceManager.hpp>
  #include <hicr/backends/mpi/L1/memoryManager.hpp>
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE
  #include <hicr/backends/host/pthreads/L1/communicationManager.hpp>
  #include <hicr/backends/host/L1/instanceManager.hpp>
  #include <hicr/backends/host/hwloc/L1/memoryManager.hpp>
#endif

#include "grid.hpp"
#include "task.hpp"

int main(int argc, char *argv[])
{
  //// Instantiating distributed execution machinery

  // Storage for the distributed engine's communication manager
  std::unique_ptr<HiCR::L1::CommunicationManager> communicationManager;

  // Storage for the distributed engine's instance manager
  std::unique_ptr<HiCR::L1::InstanceManager> instanceManager;

  // Storage for the distributed engine's memory manager
  std::unique_ptr<HiCR::L1::MemoryManager> memoryManager;

#ifdef _TASKR_DISTRIBUTED_ENGINE_LPF
  #error "LPF backend not supported yet"
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_MPI
  instanceManager      = HiCR::backend::mpi::L1::InstanceManager::createDefault(&argc, &argv);
  communicationManager = std::make_unique<HiCR::backend::mpi::L1::CommunicationManager>();
  memoryManager        = std::make_unique<HiCR::backend::mpi::L1::MemoryManager>();
#endif

#ifdef _TASKR_DISTRIBUTED_ENGINE_NONE
  instanceManager      = std::make_unique<HiCR::backend::host::L1::InstanceManager>();
  communicationManager = std::make_unique<HiCR::backend::host::pthreads::L1::CommunicationManager>();
  memoryManager        = HiCR::backend::host::hwloc::L1::MemoryManager::createDefault();
#endif

  // Creating (local host) topology manager
  const auto topologyManager = HiCR::backend::host::hwloc::L1::TopologyManager::createDefault();

  // Getting distributed instance information
  const auto instanceCount  = instanceManager->getInstances().size();
  const auto myInstanceId   = instanceManager->getCurrentInstance()->getId();
  const auto rootInstanceId = instanceManager->getRootInstanceId();
  const auto isRootInstance = myInstanceId == rootInstanceId;

  //// Setting up Taskr

  // Creating HWloc topology object
  hwloc_topology_t topology;

  // Reserving memory for hwloc
  hwloc_topology_init(&topology);

  // Initializing Pthreads-based compute manager to run tasks in parallel
  HiCR::backend::host::pthreads::L1::ComputeManager computeManager;

  // Initializing HWLoc-based host (CPU) topology manager
  HiCR::backend::host::hwloc::L1::TopologyManager tm(&topology);

  // Asking backend to check the available devices
  const auto t = tm.queryTopology();
  //printf("Topology: %s\n", t.serialize().dump(2).c_str());

  // Getting NUMA Domain information
  const auto &numaDomains = t.getDevices();
  printf("NUMA Domains per Node: %lu\n", numaDomains.size());

  // Assuming one process per numa domain
  size_t numaDomainId = myInstanceId % numaDomains.size();
  auto   numaDomain   = numaDomains[numaDomainId];
  printf("Instance %lu - Using NUMA domain: %lu\n", myInstanceId, numaDomainId);

  // Updating the compute resource list
  auto computeResources = numaDomain->getComputeResourceList();
  printf("PUs Per NUMA Domain: %lu\n", computeResources.size());

  // Creating taskr object
  taskr::Runtime taskr(&computeManager);

  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Auto-adding task upon suspend, to allow it to run as soon as it dependencies have been satisfied
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { taskr.resumeTask(task); });

  // Create processing units from the detected compute resource list and giving them to taskr
  for (auto &resource : computeResources)
  {
    // Creating a processing unit out of the computational resource
    auto processingUnit = computeManager.createProcessingUnit(resource);

    // Assigning resource to the taskr
    taskr.addProcessingUnit(std::move(processingUnit));
  }

  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  //// Setting up application configuration

  // Setting default values
  size_t  gDepth = 1;
  size_t  N      = 128;
  ssize_t nIters = 100;
  D3      pt     = D3({.x = 1, .y = 1, .z = 1});
  D3      lt     = D3({.x = 1, .y = 1, .z = 1});

  // Parsing user inputs
  for (int i = 0; i < argc; i++)
  {
    if (!strcmp(argv[i], "-px")) pt.x = atoi(argv[++i]);
    if (!strcmp(argv[i], "-py")) pt.y = atoi(argv[++i]);
    if (!strcmp(argv[i], "-pz")) pt.z = atoi(argv[++i]);
    if (!strcmp(argv[i], "-lx")) lt.x = atoi(argv[++i]);
    if (!strcmp(argv[i], "-ly")) lt.y = atoi(argv[++i]);
    if (!strcmp(argv[i], "-lz")) lt.z = atoi(argv[++i]);
    if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
    if (!strcmp(argv[i], "-i")) nIters = atoi(argv[++i]);
  }

  if ((size_t)(pt.x * pt.y * pt.z) != instanceCount)
  {
    if (isRootInstance) printf("[Error] The specified px/py/pz geometry does not match the number of instances (-n %lu).\n", instanceCount);
    instanceManager->abort(-1);
  }

  // Creating and initializing Grid
  auto g       = std::make_unique<Grid>(myInstanceId, N, nIters, gDepth, pt, lt, &taskr, memoryManager.get(), topologyManager.get(), communicationManager.get());
  bool success = g->initialize();
  if (success == false) instanceManager->abort(-1);

  // Creating reset function
  g->resetFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->reset(currentTask->i, currentTask->j, currentTask->k);
  });

  // Creating compute function
  g->computeFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->compute(currentTask->i, currentTask->j, currentTask->k, currentTask->iteration);
  });

  // Creating receive function
  g->receiveFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->receive(currentTask->i, currentTask->j, currentTask->k, currentTask->iteration);
  });

  // Creating unpack function
  g->unpackFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->unpack(currentTask->i, currentTask->j, currentTask->k, currentTask->iteration);
  });

  // Creating pack function
  g->packFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->pack(currentTask->i, currentTask->j, currentTask->k, currentTask->iteration);
  });

  // Creating send function
  g->sendFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->send(currentTask->i, currentTask->j, currentTask->k, currentTask->iteration);
  });

  // Creating residual calculation function
  g->localResidualFc = computeManager.createExecutionUnit([&g]() {
    auto currentTask = (Task *)taskr::getCurrentTask();
    g->calculateLocalResidual(currentTask->i, currentTask->j, currentTask->k, currentTask->iteration);
  });

  // Defining execution unit to run by all the instances
  instanceManager->addRPCTarget("processGrid", [&]() {
    // printf("Instance %lu: Executing...\n", myInstanceId);

    // Creating tasks to reset the grid
    for (ssize_t i = 0; i < lt.x; i++)
      for (ssize_t j = 0; j < lt.y; j++)
        for (ssize_t k = 0; k < lt.z; k++)
        {
          auto resetTask = new Task("Reset", i, j, k, 0, g->resetFc);
          taskr.addTask(resetTask);
        }

    // Initializing TaskR
    taskr.initialize();

    // Running Taskr initially
    taskr.run();

    // Creating initial set tasks to solve the first iteration
    if (nIters > 0) // Only compute if at least one iteartion is required
      for (ssize_t i = 0; i < lt.x; i++)
        for (ssize_t j = 0; j < lt.y; j++)
          for (ssize_t k = 0; k < lt.z; k++)
          {
            taskr.addTask(new Task("Compute", i, j, k, 0, g->computeFc));

            auto packTask = new Task("Pack", i, j, k, 0, g->packFc);
            packTask->addDependency(Task::encodeTaskName("Compute", i, j, k, 0));
            taskr.addTask(packTask);

            auto sendTask = new Task("Send", i, j, k, 0, g->sendFc);
            sendTask->addDependency(Task::encodeTaskName("Pack", i, j, k, 0));
            taskr.addTask(sendTask);

            auto recvTask = new Task("Receive", i, j, k, 0, g->receiveFc);
            taskr.addTask(recvTask);

            auto unpackTask = new Task("Unpack", i, j, k, 0, g->unpackFc);
            unpackTask->addDependency(Grid::encodeTaskName("Receive", i, j, k, 0));
            taskr.addTask(unpackTask);
          }

    // Setting start time as now
    auto t0 = std::chrono::high_resolution_clock::now();

    // Running Taskr
    taskr.run();


    ////// Calculating residual

    // Reset local residual to zero
    g->resetResidual();

    // Calculating local residual
    for (ssize_t i = 0; i < lt.x; i++)
      for (ssize_t j = 0; j < lt.y; j++)
        for (ssize_t k = 0; k < lt.z; k++)
        {
          auto residualTask = new Task("Residual", i, j, k, nIters, g->localResidualFc);
          taskr.addTask(residualTask);
        }

    // Running Taskr
    taskr.run();

    // Finalizing TaskR
    taskr.finalize();

    // for (size_t i = 0; i < instanceCount; i++)
    // {
    //   if (myInstanceId == i)
    //   {
    //     printf("Process: %lu, Residual: %.8f\n", myInstanceId, g->_residual.load());
    //     g->print(nIters);
    //   }
    //   printf("\n");
    //   usleep(50000);
    // }

    // If i'm not the root instance, simply send my locally calculated reisdual
    if (isRootInstance == false)
    {
      *(double *)g->residualSendBuffer->getPointer() = g->_residual;
      g->residualProducerChannel->push(g->residualSendBuffer, 1);
      g->finalize();
      return;
    }

    // Otherwise gather all the residuals and print the results
    double globalRes = g->_residual;

    for (size_t i = 0; i < instanceCount - 1; i++)
    {
      while (g->residualConsumerChannel->isEmpty())
        ;
      double *residualPtr = (double *)g->residualConsumerChannel->getTokenBuffer()->getSourceLocalMemorySlot()->getPointer() + g->residualConsumerChannel->peek(0);
      g->residualConsumerChannel->pop();
      globalRes += *residualPtr;
    }

    // Setting final time now
    auto                         tf       = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> dt       = tf - t0;
    float                        execTime = dt.count();

    double residual = sqrt(globalRes / ((double)(N - 1) * (double)(N - 1) * (double)(N - 1)));
    double gflops   = nIters * (double)N * (double)N * (double)N * (2 + gDepth * 8) / (1.0e9);
    printf("%.4fs, %.3f GFlop/s (L2 Norm: %.10g)\n", execTime, gflops / execTime, residual);

    g->finalize();
  });

  // Creating deployer instance
  std::vector<HiCR::L1::TopologyManager *> topologyManagers = {topologyManager.get()};
  auto                                     deployer         = HiCR::Deployer(instanceManager.get(), communicationManager.get(), memoryManager.get(), topologyManagers);

  // Initializing deployer (bifurcates between root and non-root instances)
  deployer.initialize();

  // Deploy the entry point function on all instances, with no topological preference
  std::vector<HiCR::MachineModel::request_t> requests = {HiCR::MachineModel::request_t{.entryPointName = "processGrid", .replicaCount = instanceCount}};
  deployer.deploy(requests);

  // Finalizing instances
  deployer.finalize();
}
