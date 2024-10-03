#pragma once

#include <atomic>
#include <lapack.h>
#include <taskr/runtime.hpp>

extern taskr::Runtime                          *_taskr;
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
    auto executionUnit = new taskr::Function([=](taskr::Task* task) { dlarnv_(&intONE, (int32_t *)&ISEED[0], &n, &matrix[i * n]); });
    auto task          = new taskr::Task(_taskCounter->fetch_add(1), executionUnit);
    _taskr->addTask(task);
  }

  _taskr->run();
  _taskr->await();

  for (int i = 0; i < n; i++)
  {
    auto executionUnit = new taskr::Function([=](taskr::Task* task) {
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
  _taskr->await();

  // Increase the diagonal by N
  for (int i = 0; i < n; i++) matrix[i + i * n] += (double)n;
}
