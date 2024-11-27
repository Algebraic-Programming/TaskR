#include <cstdio>
#include <taskr/taskr.hpp>
#include "jobs.hpp"

#define JOB_ID 1

void job1(taskr::Runtime &taskr)
{
  // Creating a storage for all the tasks we will create in this example
  std::vector<taskr::Task *> tasks(3 * ITERATIONS);

  // Creating the execution units (functions that the tasks will run)
  auto taskAfc = taskr::Function([&](taskr::Task *task) { printf("Job 1 - Task A %lu\n", task->getLabel()); });
  auto taskBfc = taskr::Function([&](taskr::Task *task) { printf("Job 1 - Task B %lu\n", task->getLabel()); });
  auto taskCfc = taskr::Function([&](taskr::Task *task) { printf("Job 1 - Task C %lu\n", task->getLabel()); });

  // Now creating tasks
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto taskId   = i * 3 + 1;
    tasks[taskId] = new taskr::Task(3 * ITERATIONS * JOB_ID + taskId, &taskBfc);
  }
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto taskId   = i * 3 + 0;
    tasks[taskId] = new taskr::Task(3 * ITERATIONS * JOB_ID + taskId, &taskAfc);
  }
  for (size_t i = 0; i < ITERATIONS; i++)
  {
    auto taskId   = i * 3 + 2;
    tasks[taskId] = new taskr::Task(3 * ITERATIONS * JOB_ID + taskId, &taskCfc);
  }

  // Now creating the dependency graph
  for (size_t i = 0; i < ITERATIONS; i++) tasks[i * 3 + 2]->addDependency(tasks[i * 3 + 1]);
  for (size_t i = 0; i < ITERATIONS; i++) tasks[i * 3 + 1]->addDependency(tasks[i * 3 + 0]);
  for (size_t i = 1; i < ITERATIONS; i++) tasks[i * 3 + 0]->addDependency(tasks[i * 3 - 1]);

  // Adding tasks to TaskR
  for (const auto task : tasks) taskr.addTask(task);
}
