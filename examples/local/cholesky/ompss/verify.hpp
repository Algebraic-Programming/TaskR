#pragma once

#include <atomic>

/**
 * Compares two matrices, one suppposed to be the ground truth
 * 
 * @param[in] expected original matrix
 * @param[in] actual matrix obtained by the Cholesky factorization
 * @param[in] matrixSize matrix dimension size
*/
bool areMatrixEqual(double *__restrict__ expected, double *__restrict__ actual, const uint32_t matrixSize)
{
  std::atomic<bool> success = true;

  for (uint32_t i = 0; i < matrixSize; i++)
  {
#pragma oss task in(expected, success)
    {
      for (uint32_t j = 0; j <= i; j++)
      {
        if (success.load() == false) { break; }
        if (expected[i * matrixSize + j] != actual[i * matrixSize + j]) { success.store(false); }
      }
    }
  }

#pragma oss taskwait

  return success.load();
}
