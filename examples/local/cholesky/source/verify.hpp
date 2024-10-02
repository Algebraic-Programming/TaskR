#pragma once

#include <atomic>

extern taskr::Runtime                          *_taskr;
extern HiCR::backend::host::L1::ComputeManager *_computeManager;
extern std::atomic<uint64_t>                   *_taskCounter;

/**
 * Compares two matrices, one suppposed to be the ground truth
 * 
 * @param[in] expected original matrix
 * @param[in] actual matrix obtained by the Cholesky factorization
 * @param[in] matrixSize matrix dimension size
*/
bool areMatrixEqual(double *__restrict__ expected, double *__restrict__ actual, const uint32_t matrixSize)
{
  std::atomic<bool> equal = true;

  for (uint32_t i = 0; i < matrixSize; i++)
  {
    auto executionUnit = _computeManager->createExecutionUnit([=, &equal]() {
      for (uint32_t j = 0; j <= i; j++)
      {
        if (equal.load() == false) { break; }
        if (expected[i * matrixSize + j] != actual[i * matrixSize + j]) { equal.store(false); }
      }
    });
    auto task          = new taskr::Task(_taskCounter->fetch_add(1), executionUnit);
    _taskr->addTask(task);
  }

  _taskr->run();
  _taskr->await();

  return equal.load();
}
