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

#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include "grid.hpp"
#include "task.hpp"

/**
 * global hash map for the taskids and hashvalue
 * first  size_t: hashvalue
 * second size_t: taskid
 */
std::unordered_map<size_t, size_t> taskid_hashmap;

bool Grid::initialize()
{
  // Inverse stencil coefficient
  invCoeff = 1.0 / (1.0 + 6.0 * (double)gDepth);

  // Checking topology correctness
  if (N % pt.x > 0)
  {
    fprintf(stderr, "Error: N (%lu) should be divisible by px (%lu)\n", N, pt.x);
    return false;
  }
  if (N % pt.y > 0)
  {
    fprintf(stderr, "Error: N (%lu) should be divisible by py (%lu)\n", N, pt.y);
    return false;
  }
  if (N % pt.z > 0)
  {
    fprintf(stderr, "Error: N (%lu) should be divisible by pz (%lu)\n", N, pt.z);
    return false;
  }

  // Calculating grid size per process
  this->ps.x = N / pt.x;
  this->ps.y = N / pt.y;
  this->ps.z = N / pt.z;

  if (ps.x % lt.x > 0)
  {
    fprintf(stderr, "Error: nx (%lu) should be divisible by lx (%lu)\n", ps.x, lt.x);
    return false;
  }
  if (ps.y % lt.y > 0)
  {
    fprintf(stderr, "Error: ny (%lu) should be divisible by ly (%lu)\n", ps.y, lt.y);
    return false;
  }
  if (ps.z % lt.z > 0)
  {
    fprintf(stderr, "Error: nz (%lu) should be divisible by lz (%lu)\n", ps.z, lt.z);
    return false;
  }

  // Calculating grid size per process plus ghost cells
  fs.x = ps.x + 2 * gDepth;
  fs.y = ps.y + 2 * gDepth;
  fs.z = ps.z + 2 * gDepth;

  U  = (double *)malloc(sizeof(double) * fs.x * fs.y * fs.z);
  Un = (double *)malloc(sizeof(double) * fs.x * fs.y * fs.z);

  processCount         = pt.x * pt.y * pt.z;
  ssize_t *globalRankX = (ssize_t *)calloc(sizeof(ssize_t), processCount);
  ssize_t *globalRankY = (ssize_t *)calloc(sizeof(ssize_t), processCount);
  ssize_t *globalRankZ = (ssize_t *)calloc(sizeof(ssize_t), processCount);

  globalProcessMapping = (ssize_t ***)calloc(sizeof(ssize_t **), pt.z);
  for (ssize_t i = 0; i < pt.z; i++) globalProcessMapping[i] = (ssize_t **)calloc(sizeof(ssize_t *), pt.y);
  for (ssize_t i = 0; i < pt.z; i++)
    for (ssize_t j = 0; j < pt.y; j++) globalProcessMapping[i][j] = (ssize_t *)calloc(sizeof(ssize_t), pt.x);

  ssize_t currentRank = 0;
  for (ssize_t z = 0; z < pt.z; z++)
    for (ssize_t y = 0; y < pt.y; y++)
      for (ssize_t x = 0; x < pt.x; x++)
      {
        globalRankZ[currentRank]      = z;
        globalRankX[currentRank]      = x;
        globalRankY[currentRank]      = y;
        globalProcessMapping[z][y][x] = currentRank;
        currentRank++;
      }

  //  if (processId == 0) for (int i = 0; i < processCount; i++) printf("Rank %d - Z: %d, Y: %d, X: %d\n", i, globalRankZ[i], globalRankY[i], globalRankX[i]);

  int curLocalRank    = 0;
  localSubGridMapping = (ssize_t ***)calloc(sizeof(ssize_t **), lt.z);
  for (ssize_t i = 0; i < lt.z; i++) localSubGridMapping[i] = (ssize_t **)calloc(sizeof(ssize_t *), lt.y);
  for (ssize_t i = 0; i < lt.z; i++)
    for (ssize_t j = 0; j < lt.y; j++) localSubGridMapping[i][j] = (ssize_t *)calloc(sizeof(ssize_t), lt.x);
  for (ssize_t i = 0; i < lt.z; i++)
    for (ssize_t j = 0; j < lt.y; j++)
      for (ssize_t k = 0; k < lt.x; k++) localSubGridMapping[i][j][k] = curLocalRank++;

  // Getting process-wise mapping
  pPos.z = globalRankZ[processId];
  pPos.y = globalRankY[processId];
  pPos.x = globalRankX[processId];

  // Grid size for local tasks
  ls.x = ps.x / lt.x;
  ls.y = ps.y / lt.y;
  ls.z = ps.z / lt.z;

  faceSizeX = ls.y * ls.z;
  faceSizeY = ls.x * ls.z;
  faceSizeZ = ls.x * ls.y;

  bufferSizeX = faceSizeX * gDepth;
  bufferSizeY = faceSizeY * gDepth;
  bufferSizeZ = faceSizeZ * gDepth;

  //////// Creating communication channel buffers

  // Asking backend to check the available devices
  const auto topology = _topologyManager->queryTopology();

  // Getting first device found
  auto device = *topology.getDevices().begin();

  // Obtaining memory spaces
  auto memSpaces = device->getMemorySpaceList();

  // Getting a reference to the first memory space
  auto firstMemorySpace = *memSpaces.begin();

  // Getting required buffer sizes
  auto consumerTokenBufferSizeX = HiCR::channel::fixedSize::Base::getTokenBufferSize(sizeof(double) * bufferSizeX, CHANNEL_DEPTH);
  auto consumerTokenBufferSizeY = HiCR::channel::fixedSize::Base::getTokenBufferSize(sizeof(double) * bufferSizeY, CHANNEL_DEPTH);
  auto consumerTokenBufferSizeZ = HiCR::channel::fixedSize::Base::getTokenBufferSize(sizeof(double) * bufferSizeZ, CHANNEL_DEPTH);

  const HiCR::GlobalMemorySlot::tag_t channelTagX0 = 0;
  const HiCR::GlobalMemorySlot::tag_t channelTagX1 = 1;
  const HiCR::GlobalMemorySlot::tag_t channelTagY0 = 2;
  const HiCR::GlobalMemorySlot::tag_t channelTagY1 = 3;
  const HiCR::GlobalMemorySlot::tag_t channelTagZ0 = 4;
  const HiCR::GlobalMemorySlot::tag_t channelTagZ1 = 5;

  std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> channelTagX0Tags;
  std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> channelTagX1Tags;
  std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> channelTagY0Tags;
  std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> channelTagY1Tags;
  std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> channelTagZ0Tags;
  std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> channelTagZ1Tags;

  // Creating tags for the exchange of channel buffers
  for (int i = 0; i < lt.z; i++)
    for (int j = 0; j < lt.y; j++)
      for (int k = 0; k < lt.x; k++)
      {
        // Allocating token buffer as a local memory slot
        auto consumerTokenBufferSlotX0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, consumerTokenBufferSizeX);
        auto consumerTokenBufferSlotY0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, consumerTokenBufferSizeY);
        auto consumerTokenBufferSlotZ0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, consumerTokenBufferSizeZ);
        auto consumerTokenBufferSlotX1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, consumerTokenBufferSizeX);
        auto consumerTokenBufferSlotY1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, consumerTokenBufferSizeY);
        auto consumerTokenBufferSlotZ1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, consumerTokenBufferSizeZ);

        // Getting required coordination buffer size
        auto coordinationBufferSize = HiCR::channel::fixedSize::Base::getCoordinationBufferSize();

        // Allocating coordination buffers as a local memory slot
        auto localConsumerCoordinationBufferX0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localConsumerCoordinationBufferY0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localConsumerCoordinationBufferZ0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localConsumerCoordinationBufferX1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localConsumerCoordinationBufferY1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localConsumerCoordinationBufferZ1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);

        auto localProducerCoordinationBufferX0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localProducerCoordinationBufferY0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localProducerCoordinationBufferZ0 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localProducerCoordinationBufferX1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localProducerCoordinationBufferY1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);
        auto localProducerCoordinationBufferZ1 = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, coordinationBufferSize);

        // Initializing coordination buffer (sets to zero the counters)
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferX0);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferY0);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferZ0);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferX1);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferY1);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferZ1);

        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferX0);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferY0);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferZ0);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferX1);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferY1);
        HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferZ1);

        const HiCR::GlobalMemorySlot::globalKey_t localTokenBufferKey        = 3 * (processId * lt.z * lt.y * lt.x + i * lt.y * lt.x + j * lt.x + k) + 0;
        const HiCR::GlobalMemorySlot::globalKey_t localCoordinationBufferKey = 3 * (processId * lt.z * lt.y * lt.x + i * lt.y * lt.x + j * lt.x + k) + 1;
        const HiCR::GlobalMemorySlot::globalKey_t localProducerBufferKey     = 3 * (processId * lt.z * lt.y * lt.x + i * lt.y * lt.x + j * lt.x + k) + 2;

        channelTagX0Tags.insert(channelTagX0Tags.begin(),
                                {{localTokenBufferKey, consumerTokenBufferSlotX0},
                                 {localCoordinationBufferKey, localConsumerCoordinationBufferX0},
                                 {localProducerBufferKey, localProducerCoordinationBufferX0}});
        channelTagX1Tags.insert(channelTagX1Tags.begin(),
                                {{localTokenBufferKey, consumerTokenBufferSlotX1},
                                 {localCoordinationBufferKey, localConsumerCoordinationBufferX1},
                                 {localProducerBufferKey, localProducerCoordinationBufferX1}});
        channelTagY0Tags.insert(channelTagY0Tags.begin(),
                                {{localTokenBufferKey, consumerTokenBufferSlotY0},
                                 {localCoordinationBufferKey, localConsumerCoordinationBufferY0},
                                 {localProducerBufferKey, localProducerCoordinationBufferY0}});
        channelTagY1Tags.insert(channelTagY1Tags.begin(),
                                {{localTokenBufferKey, consumerTokenBufferSlotY1},
                                 {localCoordinationBufferKey, localConsumerCoordinationBufferY1},
                                 {localProducerBufferKey, localProducerCoordinationBufferY1}});
        channelTagZ0Tags.insert(channelTagZ0Tags.begin(),
                                {{localTokenBufferKey, consumerTokenBufferSlotZ0},
                                 {localCoordinationBufferKey, localConsumerCoordinationBufferZ0},
                                 {localProducerBufferKey, localProducerCoordinationBufferZ0}});
        channelTagZ1Tags.insert(channelTagZ1Tags.begin(),
                                {{localTokenBufferKey, consumerTokenBufferSlotZ1},
                                 {localCoordinationBufferKey, localConsumerCoordinationBufferZ1},
                                 {localProducerBufferKey, localProducerCoordinationBufferZ1}});
      }

  // Exchanging local memory slots to become global for them to be used by the remote end
  _communicationManager->exchangeGlobalMemorySlots(channelTagX0, channelTagX0Tags);
  _communicationManager->exchangeGlobalMemorySlots(channelTagX1, channelTagX1Tags);
  _communicationManager->exchangeGlobalMemorySlots(channelTagY0, channelTagY0Tags);
  _communicationManager->exchangeGlobalMemorySlots(channelTagY1, channelTagY1Tags);
  _communicationManager->exchangeGlobalMemorySlots(channelTagZ0, channelTagZ0Tags);
  _communicationManager->exchangeGlobalMemorySlots(channelTagZ1, channelTagZ1Tags);

  // Synchronizing so that all actors have finished registering their global memory slots
  _communicationManager->fence(channelTagX0);
  _communicationManager->fence(channelTagX1);
  _communicationManager->fence(channelTagY0);
  _communicationManager->fence(channelTagY1);
  _communicationManager->fence(channelTagZ0);
  _communicationManager->fence(channelTagZ1);

  // Mapping for local tasks
  localRankCount = lt.x * lt.y * lt.z;
  subgrids.resize(localRankCount);
  for (ssize_t localId = 0; localId < localRankCount; localId++)
  {
    auto &t = subgrids[localId];

    // Processing local task mapping
    for (int i = 0; i < lt.z; i++)
      for (int j = 0; j < lt.y; j++)
        for (int k = 0; k < lt.x; k++)
          if (localSubGridMapping[i][j][k] == localId)
          {
            t.lPos.z = i;
            t.lPos.y = j;
            t.lPos.x = k;
          }

    t.lStart.x = ls.x * t.lPos.x + gDepth;
    t.lStart.y = ls.y * t.lPos.y + gDepth;
    t.lStart.z = ls.z * t.lPos.z + gDepth;

    t.lEnd.x = t.lStart.x + ls.x;
    t.lEnd.y = t.lStart.y + ls.y;
    t.lEnd.z = t.lStart.z + ls.z;

    t.X0.type      = LOCAL;
    t.X0.processId = processId;
    t.X1.type      = LOCAL;
    t.X1.processId = processId;
    t.Y0.type      = LOCAL;
    t.Y0.processId = processId;
    t.Y1.type      = LOCAL;
    t.Y1.processId = processId;
    t.Z0.type      = LOCAL;
    t.Z0.processId = processId;
    t.Z1.type      = LOCAL;
    t.Z1.processId = processId;

    t.X0.lPos.x = t.lPos.x - 1;
    t.X0.lPos.y = t.lPos.y;
    t.X0.lPos.z = t.lPos.z;
    t.X1.lPos.x = t.lPos.x + 1;
    t.X1.lPos.y = t.lPos.y;
    t.X1.lPos.z = t.lPos.z;
    t.Y0.lPos.x = t.lPos.x;
    t.Y0.lPos.y = t.lPos.y - 1;
    t.Y0.lPos.z = t.lPos.z;
    t.Y1.lPos.x = t.lPos.x;
    t.Y1.lPos.y = t.lPos.y + 1;
    t.Y1.lPos.z = t.lPos.z;
    t.Z0.lPos.x = t.lPos.x;
    t.Z0.lPos.y = t.lPos.y;
    t.Z0.lPos.z = t.lPos.z - 1;
    t.Z1.lPos.x = t.lPos.x;
    t.Z1.lPos.y = t.lPos.y;
    t.Z1.lPos.z = t.lPos.z + 1;

    if (t.X0.lPos.x == -1)
    {
      t.X0.type   = REMOTE;
      t.X0.lPos.x = lt.x - 1;
    }
    if (t.X1.lPos.x == lt.x)
    {
      t.X1.type   = REMOTE;
      t.X1.lPos.x = 0;
    }
    if (t.Y0.lPos.y == -1)
    {
      t.Y0.type   = REMOTE;
      t.Y0.lPos.y = lt.y - 1;
    }
    if (t.Y1.lPos.y == lt.y)
    {
      t.Y1.type   = REMOTE;
      t.Y1.lPos.y = 0;
    }
    if (t.Z0.lPos.z == -1)
    {
      t.Z0.type   = REMOTE;
      t.Z0.lPos.z = lt.z - 1;
    }
    if (t.Z1.lPos.z == lt.z)
    {
      t.Z1.type   = REMOTE;
      t.Z1.lPos.z = 0;
    }

    if (pPos.x == 0 && t.lPos.x == 0) t.X0.type = BOUNDARY;
    if (pPos.y == 0 && t.lPos.y == 0) t.Y0.type = BOUNDARY;
    if (pPos.z == 0 && t.lPos.z == 0) t.Z0.type = BOUNDARY;
    if (pPos.x == pt.x - 1 && t.lPos.x == lt.x - 1) t.X1.type = BOUNDARY;
    if (pPos.y == pt.y - 1 && t.lPos.y == lt.y - 1) t.Y1.type = BOUNDARY;
    if (pPos.z == pt.z - 1 && t.lPos.z == lt.z - 1) t.Z1.type = BOUNDARY;

    t.X0.localId = localSubGridMapping[t.lPos.z][t.lPos.y][t.X0.lPos.x];
    t.X1.localId = localSubGridMapping[t.lPos.z][t.lPos.y][t.X1.lPos.x];
    t.Y0.localId = localSubGridMapping[t.lPos.z][t.Y0.lPos.y][t.lPos.x];
    t.Y1.localId = localSubGridMapping[t.lPos.z][t.Y1.lPos.y][t.lPos.x];
    t.Z0.localId = localSubGridMapping[t.Z0.lPos.z][t.lPos.y][t.lPos.x];
    t.Z1.localId = localSubGridMapping[t.Z1.lPos.z][t.lPos.y][t.lPos.x];

    if (t.X0.type == REMOTE) t.X0.processId = globalProcessMapping[pPos.z][pPos.y][pPos.x - 1];
    if (t.X1.type == REMOTE) t.X1.processId = globalProcessMapping[pPos.z][pPos.y][pPos.x + 1];
    if (t.Y0.type == REMOTE) t.Y0.processId = globalProcessMapping[pPos.z][pPos.y - 1][pPos.x];
    if (t.Y1.type == REMOTE) t.Y1.processId = globalProcessMapping[pPos.z][pPos.y + 1][pPos.x];
    if (t.Z0.type == REMOTE) t.Z0.processId = globalProcessMapping[pPos.z - 1][pPos.y][pPos.x];
    if (t.Z1.type == REMOTE) t.Z1.processId = globalProcessMapping[pPos.z + 1][pPos.y][pPos.x];

    // Allocating pack buffers
    if (t.X0.type == REMOTE) t.X0PackMemorySlot = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, bufferSizeX * sizeof(double));
    if (t.X1.type == REMOTE) t.X1PackMemorySlot = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, bufferSizeX * sizeof(double));
    if (t.Y0.type == REMOTE) t.Y0PackMemorySlot = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, bufferSizeY * sizeof(double));
    if (t.Y1.type == REMOTE) t.Y1PackMemorySlot = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, bufferSizeY * sizeof(double));
    if (t.Z0.type == REMOTE) t.Z0PackMemorySlot = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, bufferSizeZ * sizeof(double));
    if (t.Z1.type == REMOTE) t.Z1PackMemorySlot = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, bufferSizeZ * sizeof(double));

    // Creating channels
    const HiCR::GlobalMemorySlot::globalKey_t localTokenBufferKey        = 3 * (processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 0;
    const HiCR::GlobalMemorySlot::globalKey_t localCoordinationBufferKey = 3 * (processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 1;
    const HiCR::GlobalMemorySlot::globalKey_t localProducerBufferKey     = 3 * (processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 2;

    if (t.X0.type == REMOTE)
    {
      // Obtaining the globally exchanged memory slots
      const HiCR::GlobalMemorySlot::globalKey_t remoteTokenBufferKey        = 3 * (t.X0.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + lt.x - 1) + 0;
      const HiCR::GlobalMemorySlot::globalKey_t remoteCoordinationBufferKey = 3 * (t.X0.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + lt.x - 1) + 1;
      const HiCR::GlobalMemorySlot::globalKey_t remoteProducerBufferKey     = 3 * (t.X0.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + lt.x - 1) + 2;

      auto sendGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagX1, remoteTokenBufferKey);
      auto sendConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX1, remoteCoordinationBufferKey);
      auto sendProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX0, localProducerBufferKey);

      auto recvGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagX0, localTokenBufferKey);
      auto recvConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX0, localCoordinationBufferKey);
      auto recvProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX1, remoteProducerBufferKey);

      // Creating producer and consumer channels
      t.X0SendChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Producer>(*_communicationManager,
                                                                                   sendGlobalTokenBufferSlot,
                                                                                   sendProducerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   sendConsumerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeX,
                                                                                   CHANNEL_DEPTH);
      t.X0RecvChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Consumer>(*_communicationManager,
                                                                                   recvGlobalTokenBufferSlot,
                                                                                   recvConsumerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   recvProducerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeX,
                                                                                   CHANNEL_DEPTH);
    }

    if (t.X1.type == REMOTE)
    {
      // Obtaining the globally exchanged memory slots
      const HiCR::GlobalMemorySlot::globalKey_t remoteTokenBufferKey        = 3 * (t.X1.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + 0) + 0;
      const HiCR::GlobalMemorySlot::globalKey_t remoteCoordinationBufferKey = 3 * (t.X1.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + 0) + 1;
      const HiCR::GlobalMemorySlot::globalKey_t remoteProducerBufferKey     = 3 * (t.X1.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + t.lPos.y * lt.x + 0) + 2;

      auto sendGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagX0, remoteTokenBufferKey);
      auto sendConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX0, remoteCoordinationBufferKey);
      auto sendProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX1, localProducerBufferKey);

      auto recvGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagX1, localTokenBufferKey);
      auto recvConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX1, localCoordinationBufferKey);
      auto recvProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagX0, remoteProducerBufferKey);

      // Creating producer and consumer channels
      t.X1SendChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Producer>(*_communicationManager,
                                                                                   sendGlobalTokenBufferSlot,
                                                                                   sendProducerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   sendConsumerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeX,
                                                                                   CHANNEL_DEPTH);
      t.X1RecvChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Consumer>(*_communicationManager,
                                                                                   recvGlobalTokenBufferSlot,
                                                                                   recvConsumerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   recvProducerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeX,
                                                                                   CHANNEL_DEPTH);
    }

    if (t.Y0.type == REMOTE)
    {
      // Obtaining the globally exchanged memory slots
      const HiCR::GlobalMemorySlot::globalKey_t remoteTokenBufferKey        = 3 * (t.Y0.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + (lt.y - 1) * lt.x + t.lPos.x) + 0;
      const HiCR::GlobalMemorySlot::globalKey_t remoteCoordinationBufferKey = 3 * (t.Y0.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + (lt.y - 1) * lt.x + t.lPos.x) + 1;
      const HiCR::GlobalMemorySlot::globalKey_t remoteProducerBufferKey     = 3 * (t.Y0.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + (lt.y - 1) * lt.x + t.lPos.x) + 2;

      auto sendGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagY1, remoteTokenBufferKey);
      auto sendConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY1, remoteCoordinationBufferKey);
      auto sendProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY0, localProducerBufferKey);

      auto recvGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagY0, localTokenBufferKey);
      auto recvConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY0, localCoordinationBufferKey);
      auto recvProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY1, remoteProducerBufferKey);

      // Creating producer and consumer channels
      t.Y0SendChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Producer>(*_communicationManager,
                                                                                   sendGlobalTokenBufferSlot,
                                                                                   sendProducerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   sendConsumerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeY,
                                                                                   CHANNEL_DEPTH);
      t.Y0RecvChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Consumer>(*_communicationManager,
                                                                                   recvGlobalTokenBufferSlot,
                                                                                   recvConsumerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   recvProducerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeY,
                                                                                   CHANNEL_DEPTH);
    }

    if (t.Y1.type == REMOTE)
    {
      // Obtaining the globally exchanged memory slots
      const HiCR::GlobalMemorySlot::globalKey_t remoteTokenBufferKey        = 3 * (t.Y1.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + (0) * lt.x + t.lPos.x) + 0;
      const HiCR::GlobalMemorySlot::globalKey_t remoteCoordinationBufferKey = 3 * (t.Y1.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + (0) * lt.x + t.lPos.x) + 1;
      const HiCR::GlobalMemorySlot::globalKey_t remoteProducerBufferKey     = 3 * (t.Y1.processId * lt.z * lt.y * lt.x + t.lPos.z * lt.y * lt.x + (0) * lt.x + t.lPos.x) + 2;

      auto sendGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagY0, remoteTokenBufferKey);
      auto sendConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY0, remoteCoordinationBufferKey);
      auto sendProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY1, localProducerBufferKey);

      auto recvGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagY1, localTokenBufferKey);
      auto recvConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY1, localCoordinationBufferKey);
      auto recvProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagY0, remoteProducerBufferKey);

      // Creating producer and consumer channels
      t.Y1SendChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Producer>(*_communicationManager,
                                                                                   sendGlobalTokenBufferSlot,
                                                                                   sendProducerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   sendConsumerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeY,
                                                                                   CHANNEL_DEPTH);
      t.Y1RecvChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Consumer>(*_communicationManager,
                                                                                   recvGlobalTokenBufferSlot,
                                                                                   recvConsumerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   recvProducerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeY,
                                                                                   CHANNEL_DEPTH);
    }

    if (t.Z0.type == REMOTE)
    {
      // Obtaining the globally exchanged memory slots
      const HiCR::GlobalMemorySlot::globalKey_t remoteTokenBufferKey        = 3 * (t.Z0.processId * lt.z * lt.y * lt.x + (lt.z - 1) * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 0;
      const HiCR::GlobalMemorySlot::globalKey_t remoteCoordinationBufferKey = 3 * (t.Z0.processId * lt.z * lt.y * lt.x + (lt.z - 1) * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 1;
      const HiCR::GlobalMemorySlot::globalKey_t remoteProducerBufferKey     = 3 * (t.Z0.processId * lt.z * lt.y * lt.x + (lt.z - 1) * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 2;

      auto sendGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagZ1, remoteTokenBufferKey);
      auto sendConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ1, remoteCoordinationBufferKey);
      auto sendProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ0, localProducerBufferKey);

      auto recvGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagZ0, localTokenBufferKey);
      auto recvConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ0, localCoordinationBufferKey);
      auto recvProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ1, remoteProducerBufferKey);

      // Creating producer and consumer channels
      t.Z0SendChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Producer>(*_communicationManager,
                                                                                   sendGlobalTokenBufferSlot,
                                                                                   sendProducerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   sendConsumerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeZ,
                                                                                   CHANNEL_DEPTH);
      t.Z0RecvChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Consumer>(*_communicationManager,
                                                                                   recvGlobalTokenBufferSlot,
                                                                                   recvConsumerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   recvProducerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeZ,
                                                                                   CHANNEL_DEPTH);
    }

    if (t.Z1.type == REMOTE)
    {
      // Obtaining the globally exchanged memory slots
      const HiCR::GlobalMemorySlot::globalKey_t remoteTokenBufferKey        = 3 * (t.Z1.processId * lt.z * lt.y * lt.x + (0) * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 0;
      const HiCR::GlobalMemorySlot::globalKey_t remoteCoordinationBufferKey = 3 * (t.Z1.processId * lt.z * lt.y * lt.x + (0) * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 1;
      const HiCR::GlobalMemorySlot::globalKey_t remoteProducerBufferKey     = 3 * (t.Z1.processId * lt.z * lt.y * lt.x + (0) * lt.y * lt.x + t.lPos.y * lt.x + t.lPos.x) + 2;

      auto sendGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagZ0, remoteTokenBufferKey);
      auto sendConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ0, remoteCoordinationBufferKey);
      auto sendProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ1, localProducerBufferKey);

      auto recvGlobalTokenBufferSlot      = _communicationManager->getGlobalMemorySlot(channelTagZ1, localTokenBufferKey);
      auto recvConsumerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ1, localCoordinationBufferKey);
      auto recvProducerCoordinationBuffer = _communicationManager->getGlobalMemorySlot(channelTagZ0, remoteProducerBufferKey);

      // Creating producer and consumer channels
      t.Z1SendChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Producer>(*_communicationManager,
                                                                                   sendGlobalTokenBufferSlot,
                                                                                   sendProducerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   sendConsumerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeZ,
                                                                                   CHANNEL_DEPTH);
      t.Z1RecvChannel = std::make_unique<HiCR::channel::fixedSize::SPSC::Consumer>(*_communicationManager,
                                                                                   recvGlobalTokenBufferSlot,
                                                                                   recvConsumerCoordinationBuffer->getSourceLocalMemorySlot(),
                                                                                   recvProducerCoordinationBuffer,
                                                                                   sizeof(double) * bufferSizeZ,
                                                                                   CHANNEL_DEPTH);
    }

    //// Creating residual channel
    const HiCR::GlobalMemorySlot::tag_t       residualChannelTag            = 6;
    const HiCR::GlobalMemorySlot::globalKey_t residualTokenBufferKey        = 0;
    const HiCR::GlobalMemorySlot::globalKey_t residualCoordinationBufferKey = 1;

    auto residualConsumerCoordinationBufferSize = HiCR::channel::fixedSize::Base::getCoordinationBufferSize();
    auto residualCoordinationBufferSlot         = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, residualConsumerCoordinationBufferSize);
    residualSendBuffer                          = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, sizeof(double));
    HiCR::channel::fixedSize::Base::initializeCoordinationBuffer(residualCoordinationBufferSlot);
    std::vector<HiCR::CommunicationManager::globalKeyMemorySlotPair_t> residualChannelTags;

    // If this is the root rank, create consumer channel elements
    if (processId == 0)
    {
      auto residualConsumerTokenBufferSize = HiCR::channel::fixedSize::Base::getTokenBufferSize(sizeof(double), processCount);
      auto residualTokenBufferSlot         = _memoryManager->allocateLocalMemorySlot(firstMemorySpace, residualConsumerTokenBufferSize);
      residualChannelTags.insert(residualChannelTags.end(), {{residualTokenBufferKey, residualTokenBufferSlot}, {residualCoordinationBufferKey, residualCoordinationBufferSlot}});
    }

    _communicationManager->exchangeGlobalMemorySlots(residualChannelTag, residualChannelTags);
    _communicationManager->fence(residualChannelTag);
    auto residualGlobalTokenBufferSlot        = _communicationManager->getGlobalMemorySlot(residualChannelTag, residualTokenBufferKey);
    auto residualGlobalCoordinationBufferSlot = _communicationManager->getGlobalMemorySlot(residualChannelTag, residualCoordinationBufferKey);

    // Creating channel
    if (processId == 0)
      residualConsumerChannel = std::make_unique<HiCR::channel::fixedSize::MPSC::locking::Consumer>(
        *_communicationManager, residualGlobalTokenBufferSlot, residualCoordinationBufferSlot, residualGlobalCoordinationBufferSlot, sizeof(double), processCount);
    if (processId != 0)
      residualProducerChannel = std::make_unique<HiCR::channel::fixedSize::MPSC::locking::Producer>(
        *_communicationManager, residualGlobalTokenBufferSlot, residualCoordinationBufferSlot, residualGlobalCoordinationBufferSlot, sizeof(double), processCount);
  }

  free(globalRankX);
  free(globalRankY);
  free(globalRankZ);

  return true;
}

void Grid::finalize()
{
  free(U);
  free(Un);

  for (ssize_t i = 0; i < pt.z; i++)
    for (ssize_t j = 0; j < pt.y; j++) free(globalProcessMapping[i][j]);
  for (ssize_t i = 0; i < pt.z; i++) free(globalProcessMapping[i]);
  free(globalProcessMapping);

  for (ssize_t i = 0; i < lt.z; i++)
    for (ssize_t j = 0; j < lt.y; j++) free(localSubGridMapping[i][j]);
  for (ssize_t i = 0; i < lt.z; i++) free(localSubGridMapping[i]);
  free(localSubGridMapping);
}

void Grid::reset(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz)
{
  auto &t = subgrids[localSubGridMapping[lz][ly][lx]];

  for (int k = t.lStart.z - gDepth; k < t.lEnd.z + gDepth; k++)
    for (int j = t.lStart.y - gDepth; j < t.lEnd.y + gDepth; j++)
      for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      {
        Un[k * fs.y * fs.x + j * fs.x + i] = 1.0;
        U[k * fs.y * fs.x + j * fs.x + i]  = 1.0;
      }

  if (t.X0.type == BOUNDARY)
    for (int i = t.lStart.y - gDepth; i < t.lEnd.y + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) U[j * fs.x * fs.y + i * fs.x + d] = 0.0;
  if (t.Y0.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) U[j * fs.x * fs.y + d * fs.x + i] = 0.0;
  if (t.Z0.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.y - gDepth; j < t.lEnd.y + gDepth; j++)
        for (int d = 0; d < gDepth; d++) U[d * fs.x * fs.y + j * fs.x + i] = 0.0;
  if (t.X1.type == BOUNDARY)
    for (int i = t.lStart.y - gDepth; i < t.lEnd.y + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) U[j * fs.x * fs.y + i * fs.x + (ps.x + gDepth + d)] = 0.0;
  if (t.Y1.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) U[j * fs.x * fs.y + (ps.y + gDepth + d) * fs.x + i] = 0.0;
  if (t.Z1.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.y - gDepth; j < t.lEnd.y + gDepth; j++)
        for (int d = 0; d < gDepth; d++) U[(ps.z + gDepth + d) * fs.x * fs.y + j * fs.x + i] = 0.0;

  if (t.X0.type == BOUNDARY)
    for (int i = t.lStart.y - gDepth; i < t.lEnd.y + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) Un[j * fs.x * fs.y + i * fs.x + d] = 0.0;
  if (t.Y0.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) Un[j * fs.x * fs.y + d * fs.x + i] = 0.0;
  if (t.Z0.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.y - gDepth; j < t.lEnd.y + gDepth; j++)
        for (int d = 0; d < gDepth; d++) Un[d * fs.x * fs.y + j * fs.x + i] = 0.0;
  if (t.X1.type == BOUNDARY)
    for (int i = t.lStart.y - gDepth; i < t.lEnd.y + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) Un[j * fs.x * fs.y + i * fs.x + (ps.x + gDepth + d)] = 0.0;
  if (t.Y1.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.z - gDepth; j < t.lEnd.z + gDepth; j++)
        for (int d = 0; d < gDepth; d++) Un[j * fs.x * fs.y + (ps.y + gDepth + d) * fs.x + i] = 0.0;
  if (t.Z1.type == BOUNDARY)
    for (int i = t.lStart.x - gDepth; i < t.lEnd.x + gDepth; i++)
      for (int j = t.lStart.y - gDepth; j < t.lEnd.y + gDepth; j++)
        for (int d = 0; d < gDepth; d++) Un[(ps.z + gDepth + d) * fs.x * fs.y + j * fs.x + i] = 0.0;
}

void Grid::compute(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
  auto  localId = localSubGridMapping[lz][ly][lx];
  auto &subGrid = subgrids[localId];

  //  printf("Running Compute (%lu, %lu, %lu, %u / %lu). Subgrid: ([%lu %lu], [%lu %lu], [%lu %lu])\n", lx, ly, lz, it, nIters, subGrid.lStart.x, subGrid.lEnd.x, subGrid.lStart.y, subGrid.lEnd.y, subGrid.lStart.z, subGrid.lEnd.z);

  // Local pointer copies
  double *localU  = it % 2 == 0 ? U : Un;
  double *localUn = it % 2 == 0 ? Un : U;

  //  printf("Rank %d running Compute (%lu, %lu, %lu, It: %u)\n", processId, lx, ly, lz, it); fflush(stdout);

  for (int k0 = subGrid.lStart.z; k0 < subGrid.lEnd.z; k0 += BLOCKZ)
  {
    int k1 = k0 + BLOCKZ < subGrid.lEnd.z ? k0 + BLOCKZ : subGrid.lEnd.z;
    for (int j0 = subGrid.lStart.y; j0 < subGrid.lEnd.y; j0 += BLOCKY)
    {
      int j1 = j0 + BLOCKY < subGrid.lEnd.y ? j0 + BLOCKY : subGrid.lEnd.y;
      for (int k = k0; k < k1; k++)
      {
        for (int j = j0; j < j1; j++)
        {
#pragma GCC ivdep
          for (int i = subGrid.lStart.x; i < subGrid.lEnd.x; i++)
          {
            double sum = localU[fs.x * fs.y * k + fs.x * j + i]; // Central
            for (int d = 1; d <= gDepth; d++)
            {
              sum += localU[fs.x * fs.y * (k - d) + fs.x * j + i]; // Z0
              sum += localU[fs.x * fs.y * (k + d) + fs.x * j + i]; // Z1
              sum += localU[fs.x * fs.y * k + fs.x * j - d + i];   // X1
              sum += localU[fs.x * fs.y * k + fs.x * j + d + i];   // X0
              sum += localU[fs.x * fs.y * k + fs.x * (j + d) + i]; // Y0
              sum += localU[fs.x * fs.y * k + fs.x * (j - d) + i]; // Y1
            }
            localUn[fs.x * fs.y * k + fs.x * j + i] = sum * invCoeff; // Update
          }
        }
      }
    }
  }
}

void Grid::receive(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
  auto  localId = localSubGridMapping[lz][ly][lx];
  auto &subGrid = subgrids[localId];

  //  printf("Rank %d running receive (%lu, %lu, %lu, It: %u)\n", processId, lx, ly, lz, it); fflush(stdout);

  if (subGrid.X0.type == REMOTE) { subGrid.X0UnpackBuffer = tryPeek(currentTask, subGrid.X0RecvChannel.get(), bufferSizeX); }
  if (subGrid.X1.type == REMOTE) { subGrid.X1UnpackBuffer = tryPeek(currentTask, subGrid.X1RecvChannel.get(), bufferSizeX); }
  if (subGrid.Y0.type == REMOTE) { subGrid.Y0UnpackBuffer = tryPeek(currentTask, subGrid.Y0RecvChannel.get(), bufferSizeY); }
  if (subGrid.Y1.type == REMOTE) { subGrid.Y1UnpackBuffer = tryPeek(currentTask, subGrid.Y1RecvChannel.get(), bufferSizeY); }
  if (subGrid.Z0.type == REMOTE) { subGrid.Z0UnpackBuffer = tryPeek(currentTask, subGrid.Z0RecvChannel.get(), bufferSizeZ); }
  if (subGrid.Z1.type == REMOTE) { subGrid.Z1UnpackBuffer = tryPeek(currentTask, subGrid.Z1RecvChannel.get(), bufferSizeZ); }
}

void Grid::unpack(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
  auto  localId = localSubGridMapping[lz][ly][lx];
  auto &subGrid = subgrids[localId];

  // Local pointer copies
  double *localU = it % 2 == 0 ? Un : U;

  //  printf("Rank %d running unpack (%lu, %lu, %lu, It: %u)\n", processId, lx, ly, lz, it); fflush(stdout);

  if (subGrid.X0.type == REMOTE)
  {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int y = 0; y < ls.y; y++) localU[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lStart.y + y) + d] = subGrid.X0UnpackBuffer[bufferIdx++];
    subGrid.X0RecvChannel->pop();
  }

  if (subGrid.X1.type == REMOTE)
  {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int y = 0; y < ls.y; y++) localU[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lStart.y + y) + subGrid.lEnd.x + d] = subGrid.X1UnpackBuffer[bufferIdx++];
    subGrid.X1RecvChannel->pop();
  }

  if (subGrid.Y0.type == REMOTE)
  {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int x = 0; x < ls.x; x++) localU[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * d + subGrid.lStart.x + x] = subGrid.Y0UnpackBuffer[bufferIdx++];
    subGrid.Y0RecvChannel->pop();
  }

  if (subGrid.Y1.type == REMOTE)
  {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int x = 0; x < ls.x; x++) localU[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lEnd.y + d) + subGrid.lStart.x + x] = subGrid.Y1UnpackBuffer[bufferIdx++];
    subGrid.Y1RecvChannel->pop();
  }

  if (subGrid.Z0.type == REMOTE)
  {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
      for (int y = 0; y < ls.y; y++)
        for (int x = 0; x < ls.x; x++) localU[fs.x * fs.y * d + fs.x * (subGrid.lStart.y + y) + (subGrid.lStart.x + x)] = subGrid.Z0UnpackBuffer[bufferIdx++];
    subGrid.Z0RecvChannel->pop();
  }

  if (subGrid.Z1.type == REMOTE)
  {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
      for (int y = 0; y < ls.y; y++)
        for (int x = 0; x < ls.x; x++) localU[fs.x * fs.y * (subGrid.lEnd.z + d) + fs.x * (subGrid.lStart.y + y) + (subGrid.lStart.x + x)] = subGrid.Z1UnpackBuffer[bufferIdx++];
    subGrid.Z1RecvChannel->pop();
  }
}

void Grid::pack(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
  auto    localId = localSubGridMapping[lz][ly][lx];
  auto   &subGrid = subgrids[localId];
  double *localUn = it % 2 == 0 ? Un : U;

  //  printf("Rank %d running pack (%lu, %lu, %lu, It: %u)\n", processId, lx, ly, lz, it); fflush(stdout);

  if (subGrid.X0.type == REMOTE)
  {
    size_t bufferIdx = 0;
    auto   bufferPtr = (double *)subGrid.X0PackMemorySlot->getPointer();
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int y = 0; y < ls.y; y++) bufferPtr[bufferIdx++] = localUn[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lStart.y + y) + (subGrid.lStart.x + d)];
  }

  if (subGrid.X1.type == REMOTE)
  {
    size_t bufferIdx = 0;
    auto   bufferPtr = (double *)subGrid.X1PackMemorySlot->getPointer();
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int y = 0; y < ls.y; y++) bufferPtr[bufferIdx++] = localUn[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lStart.y + y) + (subGrid.lEnd.x - gDepth + d)];
  }

  if (subGrid.Y0.type == REMOTE)
  {
    size_t bufferIdx = 0;
    auto   bufferPtr = (double *)subGrid.Y0PackMemorySlot->getPointer();
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int x = 0; x < ls.x; x++) bufferPtr[bufferIdx++] = localUn[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lStart.y + d) + (subGrid.lStart.x + x)];
  }

  if (subGrid.Y1.type == REMOTE)
  {
    size_t bufferIdx = 0;
    auto   bufferPtr = (double *)subGrid.Y1PackMemorySlot->getPointer();
    for (int d = 0; d < gDepth; d++)
      for (int z = 0; z < ls.z; z++)
        for (int x = 0; x < ls.x; x++) bufferPtr[bufferIdx++] = localUn[fs.x * fs.y * (subGrid.lStart.z + z) + fs.x * (subGrid.lEnd.y - gDepth + d) + (subGrid.lStart.x + x)];
  }

  if (subGrid.Z0.type == REMOTE)
  {
    size_t bufferIdx = 0;
    auto   bufferPtr = (double *)subGrid.Z0PackMemorySlot->getPointer();
    for (int d = 0; d < gDepth; d++)
      for (int y = 0; y < ls.y; y++)
        for (int x = 0; x < ls.x; x++) bufferPtr[bufferIdx++] = localUn[fs.x * fs.y * (subGrid.lStart.z + d) + fs.x * (subGrid.lStart.y + y) + (subGrid.lStart.x + x)];
  }

  if (subGrid.Z1.type == REMOTE)
  {
    size_t bufferIdx = 0;
    auto   bufferPtr = (double *)subGrid.Z1PackMemorySlot->getPointer();
    for (int d = 0; d < gDepth; d++)
      for (int y = 0; y < ls.y; y++)
        for (int x = 0; x < ls.x; x++) bufferPtr[bufferIdx++] = localUn[fs.x * fs.y * (subGrid.lEnd.z - gDepth + d) + fs.x * (subGrid.lStart.y + y) + (subGrid.lStart.x + x)];
  }
}

void Grid::send(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
  auto  localId = localSubGridMapping[lz][ly][lx];
  auto &subGrid = subgrids[localId];

  //  printf("Rank %d running send (%lu, %lu, %lu, It: %u)\n", processId, lx, ly, lz, it); fflush(stdout);

  if (subGrid.X0.type == REMOTE) tryPush(currentTask, subGrid.X0SendChannel.get(), subGrid.X0PackMemorySlot);
  if (subGrid.X1.type == REMOTE) tryPush(currentTask, subGrid.X1SendChannel.get(), subGrid.X1PackMemorySlot);
  if (subGrid.Y0.type == REMOTE) tryPush(currentTask, subGrid.Y0SendChannel.get(), subGrid.Y0PackMemorySlot);
  if (subGrid.Y1.type == REMOTE) tryPush(currentTask, subGrid.Y1SendChannel.get(), subGrid.Y1PackMemorySlot);
  if (subGrid.Z0.type == REMOTE) tryPush(currentTask, subGrid.Z0SendChannel.get(), subGrid.Z0PackMemorySlot);
  if (subGrid.Z1.type == REMOTE) tryPush(currentTask, subGrid.Z1SendChannel.get(), subGrid.Z1PackMemorySlot);
}

void Grid::calculateLocalResidual(taskr::Task *currentTask, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
  auto   &t      = subgrids[localSubGridMapping[lz][ly][lx]];
  double *localU = it % 2 == 0 ? U : Un;

  double err = 0;
  for (int k = t.lStart.z; k < t.lEnd.z; k++)
    for (int j = t.lStart.y; j < t.lEnd.y; j++)
      for (int i = t.lStart.x; i < t.lEnd.x; i++)
      {
        double r = localU[k * fs.y * fs.x + j * fs.x + i];
        err += r * r;
      }

  _residual += err;
}

void Grid::print(const uint32_t it)
{
  double *localU = it % 2 == 0 ? U : Un;

  for (ssize_t z = 0; z < fs.z; z++)
  {
    printf("Z Face %02lu\n", z);
    printf("---------------------\n");

    for (ssize_t y = 0; y < fs.y; y++)
    {
      for (ssize_t x = 0; x < fs.x; x++) { printf("%f ", localU[fs.x * fs.y * z + fs.x * y + x]); }
      printf("\n");
    }
  }
}