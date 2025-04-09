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
