#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>
#include "jobs.hpp"

#define JOB_ID 2

void job2(HiCR::backend::host::L1::ComputeManager* computeManager, taskr::Runtime& taskr)
{
  // Creating a storage for all the tasks we will create in this example
  std::vector<taskr::Task*> tasks(3 * ITERATIONS);

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = computeManager->createExecutionUnit([]() { printf("Job 2 - Task A %lu\n", taskr::getCurrentTask()->getLabel()); });
  auto taskBfc = computeManager->createExecutionUnit([]() { printf("Job 2 - Task B %lu\n", taskr::getCurrentTask()->getLabel()); });
  auto taskCfc = computeManager->createExecutionUnit([]() { printf("Job 2 - Task C %lu\n", taskr::getCurrentTask()->getLabel()); });
  
  // Now creating tasks
  for (size_t i = 0; i < ITERATIONS; i++) { auto taskId = i * 3 + 1; tasks[taskId] = new taskr::Task(3 * ITERATIONS * JOB_ID + taskId, taskBfc); }
  for (size_t i = 0; i < ITERATIONS; i++) { auto taskId = i * 3 + 0; tasks[taskId] = new taskr::Task(3 * ITERATIONS * JOB_ID + taskId, taskAfc); }
  for (size_t i = 0; i < ITERATIONS; i++) { auto taskId = i * 3 + 2; tasks[taskId] = new taskr::Task(3 * ITERATIONS * JOB_ID + taskId, taskCfc); }
  
  // Now creating the dependency graph
  for (size_t i = 0; i < ITERATIONS; i++) taskr.addDependency(tasks[i * 3 + 2], tasks[i * 3 + 1]);
  for (size_t i = 0; i < ITERATIONS; i++) taskr.addDependency(tasks[i * 3 + 1], tasks[i * 3 + 0]);
  for (size_t i = 1; i < ITERATIONS; i++) taskr.addDependency(tasks[i * 3 + 0], tasks[i * 3 - 1]);

  // Adding tasks to TaskR
  for (const auto task : tasks) taskr.addTask(task);
}
