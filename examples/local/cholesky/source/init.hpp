#pragma once

#include <atomic>
#include <lapack.h>
#include <hicr/core/L0/executionUnit.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/runtime.hpp>

extern taskr::Runtime                          *_taskr;
extern HiCR::backend::host::L1::ComputeManager *_computeManager;
extern std::atomic<uint64_t>                   *_taskCounter;

/**
 * Initialize a symmetric matrix with random generated values 
 * 
 * @param[in] matrix the pointer the the matrix memory
 * @param[in] dimension size of the matrix dimension
*/
void initMatrix(double *__restrict__ matrix, uint32_t dimension)
{
  int n        = dimension;
  int ISEED[4] = {0, 0, 0, 1};
  int intONE   = 1;

  // Get randomized numbers
  for (int i = 0; i < n; i++)
  {
    auto executionUnit = _computeManager->createExecutionUnit([=]() { dlarnv_(&intONE, (int32_t *)&ISEED[0], &n, &matrix[i * n]); });
    auto task          = new taskr::Task(_taskCounter->fetch_add(1), executionUnit);
    _taskr->addTask(task);
  }

  _taskr->run();

  for (int i = 0; i < n; i++)
  {
    auto executionUnit = _computeManager->createExecutionUnit([=]() {
      for (int j = 0; j <= i; j++)
      {
        // Make the matrix simmetrical
        matrix[j * n + i] = matrix[j * n + i] + matrix[i * n + j];
        matrix[i * n + j] = matrix[j * n + i];
      }
    });
    auto task          = new taskr::Task(_taskCounter->fetch_add(1), executionUnit);
    _taskr->addTask(task);
  }

  _taskr->run();

  // Increase the diagonal by N
  for (int i = 0; i < n; i++) matrix[i + i * n] += (double)n;
}
