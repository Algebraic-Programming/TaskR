#include <cstdio>
#include <chrono>
#include <lapack.h>
#include <cblas.h>
#include <hicr/backends/pthreads/L1/computeManager.hpp>
#include <hicr/core/L0/device.hpp>
#include <hicr/core/L0/computeResource.hpp>
#include <hicr/backends/hwloc/L1/memoryManager.hpp>
#include <taskr/taskr.hpp>

#include "../utils.hpp"
#include "init.hpp"
#include "verify.hpp"

// Global variables
/**
 * Representation of the input matrix where output dependency are written and input dependency are read
*/
extern std::vector<std::vector<std::unordered_set<taskr::Task *>>> _dependencyGrid;
extern taskr::Runtime                                             *_taskr;
extern std::atomic<uint64_t>                                      *_taskCounter;

/**
 * Reads input dependency from the dependency grid and adds it to the task's dependencies
 * 
 * @param[in] task task whose dependency needs to be updated
 * @param[in] row row of dependency grid
 * @param[in] column column of dependency grid
*/
__INLINE__ void addTaskDependency(taskr::Task *task, const uint32_t row, const uint32_t column)
{
  auto dependencies = _dependencyGrid[row][column];
  for (const auto d : dependencies) { task->addDependency(d); }
}

/**
 * Update dependency grid value with a new task label
 * 
 * @param[in] task that creates a new out dependency
 * @param[in] row row of depency grid 
 * @param[in] column column of dependency grid
*/
__INLINE__ void updateDependencyGrid(taskr::Task *task, const uint32_t row, const uint32_t column) { _dependencyGrid[row][column].emplace(task); }

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

void cholesky(taskr::Runtime &taskr, std::vector<std::vector<std::shared_ptr<HiCR::L0::LocalMemorySlot>>> &blockMatrix, const uint32_t blocks, const uint32_t blockSize)
{
  double *pointer0;
  double *pointer1;
  double *pointer2;
  for (uint32_t i = 0; i < blocks; i++)
  {
    // Diagonal Block factorization
    pointer0                = (double *)blockMatrix[i][i]->getPointer();
    auto potrfExecutionUnit = new taskr::Function([=](taskr::Task *task) { potrf(pointer0, blockSize, blockSize); });
    auto potrfTask          = new taskr::Task(_taskCounter->fetch_add(1), potrfExecutionUnit);
    addTaskDependency(potrfTask, i, i);
    updateDependencyGrid(potrfTask, i, i);
    taskr.addTask(potrfTask);

    // Triangular systems
    for (uint32_t j = i + 1; j < blocks; j++)
    {
      pointer0               = (double *)blockMatrix[i][i]->getPointer();
      pointer1               = (double *)blockMatrix[i][j]->getPointer();
      auto trsmExecutionUnit = new taskr::Function([=](taskr::Task *task) { trsm(pointer0, pointer1, blockSize, blockSize); });
      auto trsmTask          = new taskr::Task(_taskCounter->fetch_add(1), trsmExecutionUnit);
      addTaskDependency(trsmTask, i, i);
      addTaskDependency(trsmTask, i, j);
      updateDependencyGrid(trsmTask, i, j);
      taskr.addTask(trsmTask);
    }

    // Update trailing matrix
    for (uint32_t j = i + 1; j < blocks; j++)
    {
      for (uint32_t k = i + 1; k < j; k++)
      {
        pointer0               = (double *)blockMatrix[i][k]->getPointer();
        pointer1               = (double *)blockMatrix[i][j]->getPointer();
        pointer2               = (double *)blockMatrix[k][j]->getPointer();
        auto gemmExecutionUnit = new taskr::Function([=](taskr::Task *task) { gemm(pointer0, pointer1, pointer2, blockSize, blockSize); });
        auto gemmTask          = new taskr::Task(_taskCounter->fetch_add(1), gemmExecutionUnit);
        addTaskDependency(gemmTask, i, j);
        addTaskDependency(gemmTask, i, k);
        addTaskDependency(gemmTask, k, j);
        updateDependencyGrid(gemmTask, k, j);
        taskr.addTask(gemmTask);
      }

      pointer0               = (double *)blockMatrix[i][j]->getPointer();
      pointer1               = (double *)blockMatrix[j][j]->getPointer();
      auto syrkExecutionUnit = new taskr::Function([=](taskr::Task *task) { syrk(pointer0, pointer1, blockSize, blockSize); });
      auto syrkTask          = new taskr::Task(_taskCounter->fetch_add(1), syrkExecutionUnit);
      addTaskDependency(syrkTask, i, j);
      addTaskDependency(syrkTask, j, j);
      updateDependencyGrid(syrkTask, j, j);
      taskr.addTask(syrkTask);
    }
  }
}

void choleskyDriver(const uint32_t                                           matrixDimension,
                    const uint32_t                                           blocks,
                    const bool                                               readFromFile,
                    const bool                                               checkResult,
                    HiCR::backend::hwloc::L1::MemoryManager           *memoryManager,
                    HiCR::backend::pthreads::L1::CommunicationManager *communicationManager,
                    const HiCR::L0::Device::computeResourceList_t           &computeResources,
                    const std::shared_ptr<HiCR::L0::MemorySpace>            &memorySpace,
                    const std::string                                       &matrixPath)
{
  // Creating taskr object
  nlohmann::json taskrConfig;
  taskrConfig["Remember Finished Objects"] = true;
  taskr::Runtime taskr(computeResources, taskrConfig);

  // Setting onTaskFinish callback to free up task memory when it finishes
  taskr.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; });

  // Initalize TaskR
  taskr.initialize();

  // Compute the blocks for the block matrix
  const uint32_t blockSize = matrixDimension / blocks;

  // Compute matrix size
  auto matrixSize = matrixDimension * matrixDimension * sizeof(double);

  // Initialize global variables
  auto taskCounter = std::atomic<uint64_t>(0);
  _taskCounter     = &taskCounter;
  _taskr           = &taskr;

  // Initialize dependency grid (blocks * blocks)
  _dependencyGrid =
    std::vector<std::vector<std::unordered_set<taskr::Task *>>>(blocks, std::vector<std::unordered_set<taskr::Task *>>(blocks, std::unordered_set<taskr::Task *>()));

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

  // Create cholesky decomposition task graph
  cholesky(taskr, blockMatrix, blocks, blockSize);

  printf("Start...\n");
  // Run the task graph
  auto start = std::chrono::high_resolution_clock::now();
  taskr.run();
  taskr.await();
  auto end   = std::chrono::high_resolution_clock::now();
  auto delta = std::chrono::duration<double>(end - start);

  printf("Cholesky decomposition took %.4f seconds\n", delta.count());

  // Exit if the check on the result is not required
  if (checkResult == false)
  {
    freeBlockMatrix(blockMatrix, memoryManager);
    memoryManager->freeLocalMemorySlot(matrix);
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

  taskr.finalize();
}