/*
 *   Copyright 2025 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chrono>
#include <hicr/core/instanceManager.hpp>
#include <taskr/taskr.hpp>

#include "grid.hpp"
#include "task.hpp"

void jacobi3d(HiCR::InstanceManager *instanceManager,
              taskr::Runtime        &taskr,
              Grid                  *g,
              size_t                 gDepth = 1,
              size_t                 N      = 128,
              ssize_t                nIters = 100,
              D3                     pt     = D3({.x = 1, .y = 1, .z = 1}),
              D3                     lt     = D3({.x = 1, .y = 1, .z = 1}))
{
  // Getting distributed instance information
  const auto instanceCount  = instanceManager->getInstances().size();
  const auto myInstanceId   = instanceManager->getCurrentInstance()->getId();
  const auto rootInstanceId = instanceManager->getRootInstanceId();
  const auto isRootInstance = myInstanceId == rootInstanceId;

  // Initializing the Grid
  bool success = g->initialize();
  if (success == false) instanceManager->abort(-1);

  // Creating grid processing functions
  g->resetFc = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->reset(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k); });
  g->computeFc =
    std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->compute(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->receiveFc =
    std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->receive(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->unpackFc = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->unpack(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->packFc   = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->pack(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->sendFc   = std::make_unique<taskr::Function>([&g](taskr::Task *task) { g->send(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });
  g->localResidualFc = std::make_unique<taskr::Function>(
    [&g](taskr::Task *task) { g->calculateLocalResidual(task, ((Task *)task)->i, ((Task *)task)->j, ((Task *)task)->k, ((Task *)task)->iteration); });

  // Task map
  std::map<taskr::taskId_t, std::shared_ptr<taskr::Task>> _taskMap;

  printf("Instance %lu: Executing...\n", myInstanceId);

  // Creating tasks to reset the grid
  for (ssize_t i = 0; i < lt.x; i++)
    for (ssize_t j = 0; j < lt.y; j++)
      for (ssize_t k = 0; k < lt.z; k++)
      {
        auto resetTask = new Task("Reset", i, j, k, 0, g->resetFc.get());
        taskr.addTask(resetTask);
      }

  // Initializing TaskR
  taskr.initialize();

  printf("Start running\n"); fflush(stdout);

  // Running Taskr initially
  taskr.run();

  printf("Now awaiting\n"); fflush(stdout);

  // Waiting for Taskr to finish
  taskr.await();

  // Creating and adding tasks (graph nodes)
  for (ssize_t it = 0; it < nIters; it++)
    for (ssize_t i = 0; i < lt.x; i++)
      for (ssize_t j = 0; j < lt.y; j++)
        for (ssize_t k = 0; k < lt.z; k++)
        {
          auto  localId = g->localSubGridMapping[k][j][i];
          auto &subGrid = g->subgrids[localId];

          // create new specific tasks
          auto computeTask = std::make_shared<Task>("Compute", i, j, k, it, g->computeFc.get());
          auto packTask    = std::make_shared<Task>("Pack", i, j, k, it, g->packFc.get());
          auto sendTask    = std::make_shared<Task>("Send", i, j, k, it, g->sendFc.get());
          auto recvTask    = std::make_shared<Task>("Receive", i, j, k, it, g->receiveFc.get());
          auto unpackTask  = std::make_shared<Task>("Unpack", i, j, k, it, g->unpackFc.get());

          _taskMap[Task::encodeTaskName("Compute", i, j, k, it)] = computeTask;
          _taskMap[Task::encodeTaskName("Pack", i, j, k, it)]    = packTask;
          _taskMap[Task::encodeTaskName("Send", i, j, k, it)]    = sendTask;
          _taskMap[Task::encodeTaskName("Receive", i, j, k, it)] = recvTask;
          _taskMap[Task::encodeTaskName("Unpack", i, j, k, it)]  = unpackTask;

          // Creating and adding local compute task dependencies
          if (it > 0)
            if (subGrid.X0.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i - 1, j + 0, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.X1.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 1, j + 0, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.Y0.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j - 1, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.Y1.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 1, k + 0, it - 1)].get());
          if (it > 0)
            if (subGrid.Z0.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 0, k - 1, it - 1)].get());
          if (it > 0)
            if (subGrid.Z1.type == LOCAL) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 0, k + 1, it - 1)].get());
          if (it > 0) computeTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i + 0, j + 0, k + 0, it - 1)].get());

          // Adding communication-related dependencies
          if (it > 0) computeTask->addDependency(_taskMap[Task::encodeTaskName("Pack", i, j, k, it - 1)].get());
          if (it > 0) computeTask->addDependency(_taskMap[Task::encodeTaskName("Unpack", i, j, k, it - 1)].get());

          // Creating and adding receive task dependencies, from iteration 1 onwards
          if (it > 0) recvTask->addDependency(_taskMap[Task::encodeTaskName("Unpack", i, j, k, it - 1)].get());

          // Creating and adding unpack task dependencies
          unpackTask->addDependency(_taskMap[Task::encodeTaskName("Receive", i, j, k, it)].get());
          unpackTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i, j, k, it)].get());

          // Creating and adding send task dependencies, from iteration 1 onwards
          packTask->addDependency(_taskMap[Task::encodeTaskName("Compute", i, j, k, it)].get());
          if (it > 0) packTask->addDependency(_taskMap[Task::encodeTaskName("Send", i, j, k, it - 1)].get());

          // Creating and adding send task dependencies, from iteration 1 onwards
          sendTask->addDependency(_taskMap[Task::encodeTaskName("Pack", i, j, k, it)].get());

          // Adding tasks to taskr
          taskr.addTask(computeTask.get());
          if (it < nIters - 1) taskr.addTask(packTask.get());
          if (it < nIters - 1) taskr.addTask(sendTask.get());
          if (it < nIters - 1) taskr.addTask(recvTask.get());
          if (it < nIters - 1) taskr.addTask(unpackTask.get());
        }

  // Setting start time as now
  auto t0 = std::chrono::high_resolution_clock::now();

  // Running Taskr
  taskr.run();

  // Waiting for Taskr to finish
  taskr.await();

  ////// Calculating residual

  // Reset local residual to zero
  g->resetResidual();

  // Calculating local residual
  for (ssize_t i = 0; i < lt.x; i++)
    for (ssize_t j = 0; j < lt.y; j++)
      for (ssize_t k = 0; k < lt.z; k++)
      {
        auto residualTask = new Task("Residual", i, j, k, nIters, g->localResidualFc.get());
        taskr.addTask(residualTask);
      }

  // Running Taskr
  taskr.run();

  // Waiting for Taskr to finish
  taskr.await();

  // Finalizing TaskR
  taskr.finalize();

  // If i'm not the root instance, simply send my locally calculated residual
  if (isRootInstance == false)
  {
    *(double *)g->residualSendBuffer->getPointer() = g->_residual;
    g->residualProducerChannel->push(g->residualSendBuffer, 1);
  }
  else
  {
    // Otherwise gather all the residuals and print the results
    double globalRes = g->_residual;

    for (size_t i = 0; i < instanceCount - 1; i++)
    {
      while (g->residualConsumerChannel->isEmpty());
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
  }

  // Finalizing grid
  g->finalize();
}