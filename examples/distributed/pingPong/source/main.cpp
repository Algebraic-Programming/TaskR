#include <hwloc.h>
#include <taskr/taskr.hpp>
#include <hicr/core/L1/communicationManager.hpp>
#include <hicr/core/L1/instanceManager.hpp>
#include <hicr/core/L1/memoryManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
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

int main(int argc, char **argv)
{
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

  // Getting first device found
  auto d = *t.getDevices().begin();

  // Updating the compute resource list
  auto computeResources = d->getComputeResourceList();

  //// Instantiating Taskr
  taskr::Runtime taskr(computeResources);

  // Setting callback to free a task as soon as it finishes executing
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [](taskr::Task *task) { delete task; });

  // Auto-adding task upon suspend, to allow it to run as soon as it dependencies have been satisfied
  taskr.setCallbackHandler(HiCR::tasking::Task::callback_t::onTaskSuspend, [&](taskr::Task *task) { taskr.resumeTask(task); });

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

  // Creating deployer instance
  std::vector<HiCR::L1::TopologyManager *> topologyManagers = {topologyManager.get()};
  auto                                     deployer         = HiCR::Deployer(instanceManager.get(), communicationManager.get(), memoryManager.get(), topologyManagers);

  // Creating entry point
  instanceManager->addRPCTarget("doPingPong", [&]() {
    // Message to be received
    HiCR::deployer::Instance::message_t recvMsg;

    // Creating execution units
    auto rootSendFunction = taskr::Function([&](taskr::Task* task) {
      // Creating message to send
      std::string sendMsg = "Hello, Non-Root Instance";

      // Sending it to others
      for (auto &instance : instanceManager->getInstances())
        if (instance->isRootInstance() == false) deployer.getCurrentInstance()->sendMessage(instance->getId(), sendMsg.data(), sendMsg.size() + 1);
    });

    auto rootRecvFunction = taskr::Function([&](taskr::Task* task) {
      // Receiving message from others
      for (size_t i = 0; i < instanceManager->getInstances().size() - 1; i++)
      {
        // Add pending operation
        taskr::getCurrentTask()->addPendingOperation([&]() {
          recvMsg = deployer.getCurrentInstance()->recvMessageAsync();
          return recvMsg.data != nullptr;
        });

        // Suspending task until the operation is ready
        taskr::getCurrentTask()->suspend();

        // Printing message
        printf("Root Instance %03lu / %03lu received message: %s\n", myInstanceId, instanceCount, (char *)recvMsg.data);
      }
    });

    auto workerRecvFunction = taskr::Function([&](taskr::Task* task) {
      // Add pending operation
      taskr::getCurrentTask()->addPendingOperation([&]() {
        recvMsg = deployer.getCurrentInstance()->recvMessageAsync();
        return recvMsg.data != nullptr;
      });

      // Suspending task until the operation is ready
      taskr::getCurrentTask()->suspend();

      // Printing message
      printf("Worker Instance %03lu / %03lu received message: %s\n", myInstanceId, instanceCount, (char *)recvMsg.data);
    });

    auto workerSendFunction = taskr::Function([&](taskr::Task* task) {
      // Creating message to send
      std::string sendMsg = "Hello, Root Instance";

      // Sending message to root
      deployer.getCurrentInstance()->sendMessage(rootInstanceId, sendMsg.data(), sendMsg.size() + 1);
    });

    // Printing my instance info
    printf("Instance %03lu / %03lu %s has started.\n", myInstanceId, instanceCount, myInstanceId == rootInstanceId ? "(Root)" : "");

    // Declaring tasks
    auto sendTask = new taskr::Task(0, myInstanceId == rootInstanceId ? &rootSendFunction : &workerSendFunction);
    auto recvTask = new taskr::Task(1, myInstanceId == rootInstanceId ? &rootRecvFunction : &workerRecvFunction);

    // Workers: we don't send pong until receiving the root's ping
    if (myInstanceId != rootInstanceId) sendTask->addDependency(recvTask->getLabel());

    // Adding tasks
    taskr.addTask(sendTask);
    taskr.addTask(recvTask);

    // Initializing TaskR
    taskr.initialize();

    // Running TaskR
    taskr.run();

    // Waiting for Taskr to finish
    taskr.await();

    // Finalizing TaskR
    taskr.finalize();
  });

  // Initializing deployer (bifurcates between root and non-root instances)
  deployer.initialize();

  //// (Root-only from now on)

  // Deploy the entry point function on all instances, with no topological preference
  std::vector<HiCR::MachineModel::request_t> requests = {HiCR::MachineModel::request_t{.entryPointName = "doPingPong", .replicaCount = instanceCount}};
  deployer.deploy(requests);

  // Finalizing worker instances
  deployer.finalize();

  return 0;
}
