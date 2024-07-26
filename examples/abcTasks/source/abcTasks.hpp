#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/runtime.hpp>

#define ITERATIONS 10

class abcTask : public HiCR::tasking::Task
{

  public:

  typedef uint64_t label_t;

  abcTask(const label_t label, std::shared_ptr<HiCR::L0::ExecutionUnit> executionUnit, HiCR::tasking::Task::taskCallbackMap_t* callbackMap) :
    HiCR::tasking::Task(executionUnit, callbackMap),
    _label(label)
    { }

  label_t getLabel() const { return _label; }

  private: 
  
  const label_t _label;
};


void abcTasks(HiCR::backend::host::L1::ComputeManager *computeManager, const HiCR::L0::Device::computeResourceList_t &computeResources)
{
  // Initializing taskr
  taskr::Runtime taskr;

  // Assigning processing resources to TaskR
  for (const auto &computeResource : computeResources) taskr.addProcessingUnit(computeManager->createProcessingUnit(computeResource));

  // Counter for the number of active tasks
  std::atomic<size_t> taskCount = 0;

  // Creating callback map for task events
  HiCR::tasking::Task::taskCallbackMap_t callbackMap;

  // Setting callback for finishing
  callbackMap.setCallback(HiCR::tasking::Task::callback_t::onTaskFinish, [&](HiCR::tasking::Task *task)
  {
     // Reduce the active task counter;
    taskCount--;

    // If this is the last task, we can finish now
    if (taskCount == 0) taskr.finalize();

    // Free-up memory now the task is finished 
    delete task;
  });

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = computeManager->createExecutionUnit([&]() { printf("Task A %lu\n", ((abcTask*)taskr.getCurrentTask())->getLabel()); });
  auto taskBfc = computeManager->createExecutionUnit([&]() { printf("Task B %lu\n", ((abcTask*)taskr.getCurrentTask())->getLabel()); });
  auto taskCfc = computeManager->createExecutionUnit([&]() { printf("Task C %lu\n", ((abcTask*)taskr.getCurrentTask())->getLabel()); });
  
  // Now creating the dependency graph
  // for (size_t i = 0; i < ITERATIONS; i++) taskr.addDependency(i * 3 + 2, i * 3 + 1);
  // for (size_t i = 0; i < ITERATIONS; i++) taskr.addDependency(i * 3 + 1, i * 3 + 0);
  // for (size_t i = 1; i < ITERATIONS; i++) taskr.addDependency(i * 3 + 0, i * 3 - 1);

  // Now creating tasks
  for (size_t i = 0; i < ITERATIONS; i++) { taskr.addTask(new abcTask(i + 3 + 2, taskCfc, &callbackMap)); taskCount++; }
  for (size_t i = 0; i < ITERATIONS; i++) { taskr.addTask(new abcTask(i * 3 + 1, taskBfc, &callbackMap)); taskCount++; } 
  for (size_t i = 0; i < ITERATIONS; i++) { taskr.addTask(new abcTask(i * 3 + 0, taskAfc, &callbackMap)); taskCount++; } 
  
  // Running taskr
  taskr.run(computeManager);
}
