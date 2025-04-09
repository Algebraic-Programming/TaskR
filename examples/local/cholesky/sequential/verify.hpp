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

#include <cblas.h>
#include <hicr/backends/host/hwloc/L1/memoryManager.hpp>
#include <hicr/core/L0/memorySpace.hpp>
#include <hicr/core/L0/localMemorySlot.hpp>

/**
 * Verify Cholesky factorization. It multiplies the result matrix with its transpose and 
 * compares the result with the original matrix.
 * 
 * @param[in] originalMatrixPtr original matrix
 * @param[in] decomposedMatrix matrix obtained by the Cholesky factorization
 * @param[in] matrixSize matrix dimension size
 * @param[in] memoryManager memory manager
 * @param[in] memorySpace memorySpace where allocation should be performed
*/
double verifyCholesky(double                                       *originalMatrixPtr,
                      double                                       *decomposedMatrix,
                      int                                           matrixSize,
                      HiCR::L1::MemoryManager                      *memoryManager,
                      const std::shared_ptr<HiCR::L0::MemorySpace> &memorySpace)
{
  // Allocate memory for reconstructedMatrix'
  auto reconstructedMatrix    = memoryManager->allocateLocalMemorySlot(memorySpace, matrixSize * matrixSize * sizeof(double));
  auto reconstructedMatrixPtr = ((double *)reconstructedMatrix->getPointer());

  // Initialize reconstructedMatrix' to zero
  for (int i = 0; i < matrixSize * matrixSize; ++i) { reconstructedMatrixPtr[i] = 0.0; }

  // Multiply decomposedMatrix by its transpose
  cblas_dgemm(CblasRowMajor,
              CblasNoTrans,
              CblasTrans,
              matrixSize,
              matrixSize,
              matrixSize,
              1.0,
              decomposedMatrix,
              matrixSize,
              decomposedMatrix,
              matrixSize,
              0.0,
              reconstructedMatrixPtr,
              matrixSize);

  // Compute the norm
  double norm = 0.0;
  for (int i = 0; i < matrixSize * matrixSize; ++i)
  {
    auto val = originalMatrixPtr[i] - reconstructedMatrixPtr[i];
    norm += val * val;
  }

  // Deallocate originalMatrixreconstructedMatrix'
  memoryManager->freeLocalMemorySlot(reconstructedMatrix);
  return std::sqrt(norm);
}
