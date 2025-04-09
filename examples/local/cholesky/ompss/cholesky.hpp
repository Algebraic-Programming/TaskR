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

#include <cstdio>
#include <chrono>
#include <lapack.h>
#include <cblas.h>
#include <hicr/backends/host/pthreads/communicationManager.hpp>
#include <hicr/backends/host/hwloc/memoryManager.hpp>

#include "../utils.hpp"
#include "init.hpp"
#include "verify.hpp"

extern uint8_t *_dependencyGrid;
extern uint64_t _blocks;

__INLINE__ void potrf(double *A, const uint32_t blockSize, const uint32_t matrixDimensionSize)
{
  char UPLO = 'L';
  int  info = 0;
  int  bs   = blockSize;
  int  ms   = matrixDimensionSize;
  LAPACK_dpotrf2(&UPLO, &bs, A, &ms, &info);
  if (info != 0) HICR_THROW_RUNTIME("dpotrf2 failed: %d", info);
}

__INLINE__ void trsm(double *A, double *B, const uint32_t blockSize, const uint32_t matrixDimensionSize)
{
  cblas_dtrsm(CblasRowMajor, CblasLeft, CblasUpper, CblasTrans, CblasNonUnit, blockSize, blockSize, 1.0, A, matrixDimensionSize, B, matrixDimensionSize);
}

__INLINE__ void gemm(double *A, double *B, double *C, const uint32_t blockSize, const uint32_t matrixDimensionSize)
{
  cblas_dgemm(CblasRowMajor, CblasTrans, CblasNoTrans, blockSize, blockSize, blockSize, -1.0, A, matrixDimensionSize, B, matrixDimensionSize, 1.0, C, matrixDimensionSize);
}

__INLINE__ void syrk(double *A, double *B, const uint32_t blockSize, const uint32_t matrixDimensionSize)
{
  cblas_dsyrk(CblasRowMajor, CblasUpper, CblasTrans, blockSize, blockSize, -1.0, A, matrixDimensionSize, 1.0, B, matrixDimensionSize);
}

void cholesky(const std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> &blockMatrix, const uint32_t blocks, const uint32_t blockSize)
{
  for (uint32_t i = 0; i < blocks; i++)
  {
// Diagonal Block factorization
#pragma oss task depend(inout : _dependencyGrid[i * _blocks + i]) label("cholesky")
    potrf((double *)blockMatrix[i][i]->getPointer(), blockSize, blockSize);

    // Triangular systems
    for (uint32_t j = i + 1; j < blocks; j++)
    {
#pragma oss task depend(in : _dependencyGrid[i * _blocks + i]) depend(inout : _dependencyGrid[i * _blocks + j]) label("trsm")
      trsm((double *)blockMatrix[i][i]->getPointer(), (double *)blockMatrix[i][j]->getPointer(), blockSize, blockSize);
    }

    // Update trailing matrix
    for (uint32_t j = i + 1; j < blocks; j++)
    {
      for (uint32_t k = i + 1; k < j; k++)
      {
#pragma oss task depend(in : _dependencyGrid[i * _blocks + k]) depend(in : _dependencyGrid[i * _blocks + j]) depend(inout : _dependencyGrid[k * _blocks + j]) label("gemm")
        gemm((double *)blockMatrix[i][k]->getPointer(), (double *)blockMatrix[i][j]->getPointer(), (double *)blockMatrix[k][j]->getPointer(), blockSize, blockSize);
      }

#pragma oss task depend(in : _dependencyGrid[i * _blocks + j]) depend(inout : _dependencyGrid[j * _blocks + j]) label("syrk")
      syrk((double *)blockMatrix[i][j]->getPointer(), (double *)blockMatrix[j][j]->getPointer(), blockSize, blockSize);
    }
  }

#pragma oss taskwait
}

void choleskyDriver(const uint32_t                                           matrixDimension,
                    const uint32_t                                           blocks,
                    const bool                                               readFromFile,
                    const bool                                               checkResult,
                    HiCR::backend::host::hwloc::MemoryManager           *memoryManager,
                    HiCR::backend::host::pthreads::CommunicationManager *communicationManager,
                    const std::shared_ptr<HiCR::L0::MemorySpace>            &memorySpace,
                    const std::string                                       &matrixPath)
{
  // Compute the blocks for the block matrix
  const uint32_t blockSize = matrixDimension / blocks;

  // Compute matrix size
  auto matrixSize = matrixDimension * matrixDimension * sizeof(double);

  // Init global variables
  auto dependencyGrid = memoryManager->allocateLocalMemorySlot(memorySpace, blocks * blocks * sizeof(uint8_t));
  _dependencyGrid     = (uint8_t *)dependencyGrid->getPointer();
  _blocks             = blocks;

  // Allocate matrix
  auto matrix = memoryManager->allocateLocalMemorySlot(memorySpace, matrixSize);

  // Populate matrix from file
  if (readFromFile == true)
  {
    auto inputPath = matrixPath + "/input/matrix-" + std::to_string(matrixDimension);
    readMatrixFromFile((double *)matrix->getPointer(), matrixSize, inputPath);
  }
  else { initMatrix((double *)matrix->getPointer(), matrixDimension); }

  // Allocate buffer to save the inital matrix if we need to check the result later
  std::shared_ptr<HiCR::L0::LocalMemorySlot> originalMatrix;

  if (checkResult == true)
  {
    // Allocate matrix
    originalMatrix = memoryManager->allocateLocalMemorySlot(memorySpace, matrixSize);

    // Read ground truth matrix from file
    auto groundTruthPath = matrixPath + "/output/matrix-" + std::to_string(matrixDimension);
    readMatrixFromFile((double *)originalMatrix->getPointer(), matrixSize, groundTruthPath);
  }

  // Initialize the block matrix
  std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> blockMatrix(blocks, std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>(blocks));

  // Malloc the block matrix memory regions
  allocateBlockMatrix(blocks, blockSize, memoryManager, memorySpace, blockMatrix);

  // Convert the original matrix (linear) to a block matrix. Each block has a size of blockSize * blockSize
  convertToBlockMatrix((double *)matrix->getPointer(), matrixDimension, blockSize, blockMatrix);

  printf("Start...\n");
  // Run the task graph
  auto start = std::chrono::high_resolution_clock::now();
  cholesky(blockMatrix, blocks, blockSize);
  auto end   = std::chrono::high_resolution_clock::now();
  auto delta = std::chrono::duration<double>(end - start);

  printf("Cholesky decomposition took %.4f seconds\n", delta.count());

  // Exit if the check on the result is not required
  if (checkResult == false)
  {
    freeBlockMatrix(blockMatrix, memoryManager);
    memoryManager->freeLocalMemorySlot(matrix);
    memoryManager->freeLocalMemorySlot(dependencyGrid);
    return;
  }

  // Check the result of Cholesky decomposition
  // Transform the matrix from the block format to a linear one
  convertToPlainMatrix((double *)matrix->getPointer(), matrixDimension, blockSize, blockMatrix);

  // Make the matrix lower triangular
  for (uint32_t i = 0; i < matrixDimension; i++)
  {
    for (uint32_t j = i + 1; j < matrixDimension; j++)
    {
      ((double *)matrix->getPointer())[(j * matrixDimension) + i] = ((double *)matrix->getPointer())[(i * matrixDimension) + j];
      ((double *)matrix->getPointer())[(i * matrixDimension) + j] = 0.0;
    }
  }

  // Verify result
  auto equal = areMatrixEqual((double *)originalMatrix->getPointer(), (double *)matrix->getPointer(), matrixDimension);
  if (equal == false) { fprintf(stderr, "factorization failed. Actual matrix and ground truth are not equal\n"); }

  // Free matrix memory
  freeBlockMatrix(blockMatrix, memoryManager);
  memoryManager->freeLocalMemorySlot(matrix);
  memoryManager->freeLocalMemorySlot(originalMatrix);
  memoryManager->freeLocalMemorySlot(dependencyGrid);
}