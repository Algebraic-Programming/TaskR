#pragma once

#include <cblas.h>
#include <fstream>
#include <hicr/core/exceptions.hpp>
#include <hicr/core/L0/localMemorySlot.hpp>
#include <hicr/core/L0/memorySpace.hpp>
#include <hicr/backends/host/hwloc/L1/memoryManager.hpp>

///////////////////////////////////// Matrix utilities

/**
 * Initialize a symmetric matrix with random generated values
 *
 * @param[in] matrix the pointer the the matrix memory
 * @param[in] dimension size of the matrix dimension
*/
void initMatrix(double *__restrict__ matrix, uint32_t dimension);

/**
 * Create a block representation of the matrix. It populates a nested vector of blocks. 
 * Each block has a size of blockDimensionSize * blockDimensionSize
 * 
 * @param[in] blocks the number of blocks to create
 * @param[in] blockDimensionSize dimension of a single block
 * @param[in] memoryManager memory manager
 * @param[in] memorySpace memorySpace where allocation should be performed
 * @param[out] blockMatrix data structure holding the resulting block matrix
*/
void allocateBlockMatrix(const uint32_t                                                        blocks,
                         const uint32_t                                                        blockDimensionSize,
                         HiCR::backend::host::hwloc::L1::MemoryManager                        *memoryManager,
                         const std::shared_ptr<HiCR::L0::MemorySpace>                         &memorySpace,
                         std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> &blockMatrix)
{
  for (uint32_t i = 0; i < blocks; i++)
    for (uint32_t j = 0; j < blocks; j++) { blockMatrix[i][j] = memoryManager->allocateLocalMemorySlot(memorySpace, blockDimensionSize * blockDimensionSize * sizeof(double)); }
}

/**
 * Free the block matrix
 * 
 * @param[in] blockMatrix data structure holding the block matrix
 * @param[in] memoryManager memory manager
*/
void freeBlockMatrix(std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> &blockMatrix, HiCR::backend::host::hwloc::L1::MemoryManager *memoryManager)
{
  for (auto &row : blockMatrix)
    for (auto &block : row) { memoryManager->freeLocalMemorySlot(block); }
}

/**
 * Convert the src matrix to block format
 * 
 * @param[in] src original matrix
 * @param[in] srcDimensionSize size of leading dimension of src matrix
 * @param[in] blockDimensionSize size of dimension of a single block
 * @param[out] dst block matrix data structure
*/
void convertToBlockMatrix(double *src, const uint32_t srcDimensionSize, const uint32_t blockDimensionSize, std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> &dst)
{
  uint32_t blocks = srcDimensionSize / blockDimensionSize;

  // Populate each block with the corresponding values from originalMatrixPtr
  for (uint32_t i = 0; i < blocks; i++)
  {
    for (uint32_t j = 0; j < blocks; j++)
    {
      for (uint32_t ii = 0; ii < blockDimensionSize; ii++)
      {
        for (uint32_t jj = 0; jj < blockDimensionSize; jj++)
        {
          uint32_t globalRow                   = i * blockDimensionSize + ii;
          uint32_t globalCol                   = j * blockDimensionSize + jj;
          auto     dstPtr                      = (double *)dst[i][j]->getPointer();
          dstPtr[ii * blockDimensionSize + jj] = src[globalRow * srcDimensionSize + globalCol];
        }
      }
    }
  }
}

/**
 * Convert the src block matrix to the linear one 
 * 
 * @param[in] dst original matrix
 * @param[in] dstDimensionSize size of leading dimension of src matrix
 * @param[in] blockDimensionSize size of dimension of a single block
 * @param[out] src block matrix data structure
*/
void convertToPlainMatrix(double *dst, const uint32_t dstDimensionSize, const uint32_t blockDimensionSize, std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> &src)
{
  auto blocks = dstDimensionSize / blockDimensionSize;
  for (uint32_t i = 0; i < blocks; i++)
  {
    for (uint32_t j = 0; j < blocks; j++)
    {
      auto srcPointer = (double *)src.at(i).at(j)->getPointer();
      auto dstPointer = &dst[(i * blockDimensionSize * dstDimensionSize) + j * blockDimensionSize];
      for (uint32_t k = 0; k < blockDimensionSize; k++)
      {
        for (uint32_t z = 0; z < blockDimensionSize; z++) { dstPointer[((k * dstDimensionSize) + z)] = srcPointer[(k * blockDimensionSize) + z]; }
      }
    }
  }
}

///////////////////////////////////// File utilities

/**
 * Write matrix to file in binary format
 * 
 * @param[in] matrix
 * @param[in] size
 * @param[in] path
*/
void writeMatrixToFile(const double *matrix, uint32_t size, const std::string &path)
{
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open()) { HICR_THROW_RUNTIME("Error opening %s", path.c_str()); }

  file.write(reinterpret_cast<const char *>(matrix), size);

  file.close();
}

/**
 * Read matrix from binary file
 * 
 * @param[out] matrix
 * @param[in] size
 * @param[in] path
*/
void readMatrixFromFile(double *matrix, uint32_t size, const std::string &path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) { HICR_THROW_RUNTIME("Error opening %s", path.c_str()); }

  file.read(reinterpret_cast<char *>(matrix), size);

  file.close();
}

/**
 * Either read matrix from a file or generate one.
 * 
 * @param[out] matrix
 * @param[in] matrixDimension
 * @param[in] matrixSize
 * @param[in] path
*/
void tryReadMatrixFromFileOrGenerate(std::shared_ptr<HiCR::L0::LocalMemorySlot> &matrix, const uint32_t matrixDimension, const uint32_t matrixSize, const std::string &path)
{
  auto file = std::ifstream(path);
  if (file.good())
  {
    printf("read matrix file\n");
    readMatrixFromFile((double *)matrix->getPointer(), matrixSize, path);
  }
  else
  {
    printf("Generate matrix\n");
    initMatrix((double *)matrix->getPointer(), matrixDimension);
    writeMatrixToFile((double *)matrix->getPointer(), matrixSize, path);
  }
}

/**
 * Write matrix to file if it does not already exist
 * 
 * @param[in] matrix
 * @param[in] matrixSize 
 * @param[in] path
*/
void tryWriteMatrixToFile(std::shared_ptr<HiCR::L0::LocalMemorySlot> &matrix, const uint32_t matrixSize, const std::string &path)
{
  auto file = std::ifstream(path);
  if (file.good())
  {
    printf("File already exists\n");
    return;
  }

  printf("write matrix to file\n");
  writeMatrixToFile((double *)matrix->getPointer(), matrixSize, path);
}
