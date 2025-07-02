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

#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <taskr/taskr.hpp>
#include <hicr/core/memoryManager.hpp>
#include <hicr/core/topologyManager.hpp>
#include <hicr/frontends/channel/fixedSize/spsc/consumer.hpp>
#include <hicr/frontends/channel/fixedSize/spsc/producer.hpp>
#include <hicr/frontends/channel/fixedSize/mpsc/locking/producer.hpp>
#include <hicr/frontends/channel/fixedSize/mpsc/locking/consumer.hpp>

#define CHANNEL_DEPTH 10
const int BLOCKZ = 96;
const int BLOCKY = 64;

extern std::unordered_map<size_t, size_t> taskid_hashmap;

enum commType
{
  REMOTE = 0,
  LOCAL,
  BOUNDARY
};

struct D3
{
  ssize_t x;
  ssize_t y;
  ssize_t z;
};

struct Neighbor
{
  commType type;
  ssize_t  processId;
  ssize_t  localId;
  D3       lPos;
};

struct SubGrid
{
  Neighbor X0, X1, Y0, Y1, Z0, Z1;
  D3       lPos;
  D3       lStart;
  D3       lEnd;

  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> X0RecvChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> X1RecvChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> Y0RecvChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> Y1RecvChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> Z0RecvChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> Z1RecvChannel;

  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> X0SendChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> X1SendChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> Y0SendChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> Y1SendChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> Z0SendChannel;
  std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> Z1SendChannel;

  std::shared_ptr<HiCR::LocalMemorySlot> X0PackMemorySlot;
  std::shared_ptr<HiCR::LocalMemorySlot> X1PackMemorySlot;
  std::shared_ptr<HiCR::LocalMemorySlot> Y0PackMemorySlot;
  std::shared_ptr<HiCR::LocalMemorySlot> Y1PackMemorySlot;
  std::shared_ptr<HiCR::LocalMemorySlot> Z0PackMemorySlot;
  std::shared_ptr<HiCR::LocalMemorySlot> Z1PackMemorySlot;

  double *X0UnpackBuffer;
  double *X1UnpackBuffer;
  double *Y0UnpackBuffer;
  double *Y1UnpackBuffer;
  double *Z0UnpackBuffer;
  double *Z1UnpackBuffer;
};

class Grid
{
  public:

  // Configuration
  const int    processId; // Id for the current global process
  size_t       processCount;
  const size_t nIters;   // Number of iterations
  const size_t N;        // Grid ls per side (N)
  const int    gDepth;   // Ghost cell depth
  double       invCoeff; // Pre-calculated inverse coefficient

  // Grid containers
  double *U;
  double *Un;

  // Grid Topology Management
  D3         ps;                   // Grid Size per Process
  const D3   pt;                   // Global process topology
  const D3   lt;                   // Local task topology
  D3         fs;                   // Effective Grid Size (including ghost cells)
  D3         ls;                   // Local Grid Size
  D3         pPos;                 // process location information
  ssize_t ***globalProcessMapping; // MPI process mapping
  ssize_t    localRankCount;

  // Local subgrid information
  ssize_t           ***localSubGridMapping;
  std::vector<SubGrid> subgrids;

  // Number of elements per face
  size_t faceSizeX;
  size_t faceSizeY;
  size_t faceSizeZ;

  // Size of MPI buffers
  size_t bufferSizeX;
  size_t bufferSizeY;
  size_t bufferSizeZ;

  // Storage for L2 residual calculation
  std::shared_ptr<HiCR::LocalMemorySlot>                             residualSendBuffer;
  std::unique_ptr<HiCR::channel::fixedSize::MPSC::locking::Consumer> residualConsumerChannel;
  std::unique_ptr<HiCR::channel::fixedSize::MPSC::locking::Producer> residualProducerChannel;
  std::atomic<double>                                                _residual;

  // Execution unit definitions
  std::unique_ptr<taskr::Function> resetFc;
  std::unique_ptr<taskr::Function> computeFc;
  std::unique_ptr<taskr::Function> receiveFc;
  std::unique_ptr<taskr::Function> unpackFc;
  std::unique_ptr<taskr::Function> packFc;
  std::unique_ptr<taskr::Function> sendFc;
  std::unique_ptr<taskr::Function> localResidualFc;

  Grid(const int                         processId,
       const size_t                      N,
       const size_t                      nIters,
       const size_t                      gDepth,
       const D3                         &pt,
       const D3                         &lt,
       taskr::Runtime *const             taskr,
       HiCR::MemoryManager *const        memoryManager,
       HiCR::TopologyManager *const      topologyManager,
       HiCR::CommunicationManager *const communicationManager)
    : processId(processId),
      nIters(nIters),
      N(N),
      gDepth(gDepth),
      pt(pt),
      lt(lt),
      _taskr(taskr),
      _memoryManager(memoryManager),
      _topologyManager(topologyManager),
      _communicationManager(communicationManager)
  {}

  bool initialize();
  void finalize();
  void print(const uint32_t it);

  void compute(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
  void receive(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
  void send(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
  void pack(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
  void unpack(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
  void reset(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz);

  void resetResidual() { _residual = 0.0; }
  void calculateLocalResidual(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
  void sync();

  static inline void tryPush(taskr::Task *currentTask, HiCR::channel::fixedSize::SPSC::Producer *channel, std::shared_ptr<HiCR::LocalMemorySlot> slot)
  {
    // If the channel is full, suspend task until it frees up
    if (channel->isFull())
    {
      // Adding pending operation: channel being freed up
      currentTask->addPendingOperation([&]() { return channel->isFull() == false; });

      // Suspending until the operation is finished
      currentTask->suspend();
    }

    // Otherwise go ahead and push
    channel->push(slot);
  }

  static double_t *tryPeek(taskr::Task *currentTask, HiCR::channel::fixedSize::SPSC::Consumer *channel, const size_t tokenSize)
  {
    // If the channel is full, suspend task until it frees up
    if (channel->isEmpty())
    {
      // Adding pending operation: channel being freed up
      currentTask->addPendingOperation([&]() { return channel->isEmpty() == false; });

      // Suspending until the operation is finished
      currentTask->suspend();
    }

    // Otherwise go ahead and push
    return (double_t *)channel->getTokenBuffer()->getSourceLocalMemorySlot()->getPointer() + channel->peek(0) * tokenSize;
  }

  inline uint64_t getLocalTaskId(const uint64_t lx, const uint64_t ly, const uint64_t lz) { return lz * ls.y * ls.x + ly * ls.x + lx; }

  taskr::Runtime *const _taskr;

  HiCR::MemoryManager *const        _memoryManager;
  HiCR::TopologyManager *const      _topologyManager;
  HiCR::CommunicationManager *const _communicationManager;

}; // class Grid
