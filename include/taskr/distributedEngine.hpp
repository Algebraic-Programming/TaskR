/*
 * Copyright Huawei Technologies Switzerland AG
 * All rights reserved.
 */

/**
 * @file instance.hpp
 * @brief Provides a definition for the TaskR distributed instance class
 * @author S. M. Martin
 * @date 8/6/2024
 */

#pragma once

#define _HICR_DEPLOYER_CHANNEL_PAYLOAD_CAPACITY 1048576
#define _HICR_DEPLOYER_CHANNEL_COUNT_CAPACITY 1024
#define _HICR_DEPLOYER_CHANNEL_BASE_TAG 0xF0000000
#define _HICR_DEPLOYER_CHANNEL_CONSUMER_SIZES_BUFFER_TAG _HICR_DEPLOYER_CHANNEL_BASE_TAG + 0
#define _HICR_DEPLOYER_CHANNEL_CONSUMER_PAYLOAD_BUFFER_TAG _HICR_DEPLOYER_CHANNEL_BASE_TAG + 1
#define _HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_SIZES_TAG _HICR_DEPLOYER_CHANNEL_BASE_TAG + 3
#define _HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_PAYLOADS_TAG _HICR_DEPLOYER_CHANNEL_BASE_TAG + 4
#define _HICR_DEPLOYER_CHANNEL_PRODUCER_COORDINATION_BUFFER_SIZES_TAG _HICR_DEPLOYER_CHANNEL_BASE_TAG + 5
#define _HICR_DEPLOYER_CHANNEL_PRODUCER_COORDINATION_BUFFER_PAYLOADS_TAG _HICR_DEPLOYER_CHANNEL_BASE_TAG + 6

#include <list>
#include <vector>
#include <algorithm>
#include <mutex>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <hicr/core/L1/instanceManager.hpp>
#include <hicr/core/L1/memoryManager.hpp>
#include <hicr/core/L1/communicationManager.hpp>
#include <hicr/core/L1/topologyManager.hpp>
#include <hicr/backends/host/hwloc/L1/topologyManager.hpp>
#include <hicr/frontends/machineModel/machineModel.hpp>
#include <hicr/frontends/channel/variableSize/mpsc/locking/producer.hpp>
#include <hicr/frontends/channel/variableSize/mpsc/locking/consumer.hpp>

namespace taskr
{

typedef HiCR::channel::variableSize::MPSC::locking::Consumer ConsumerChannel;
typedef HiCR::channel::variableSize::MPSC::locking::Producer ProducerChannel;

/**
 * Defines the distributed engine class, which enables channel- and data object-based communication
 */
class DistributedEngine
{
  public:

  /**
   * Storage for inter-instance message information
  */
  struct message_t
  {
    /// Pointer to the message's data allocation
    const void *data = nullptr;

    /// Size of the message
    size_t size = 0;
  };

  DistributedEngine() = delete;

  /**
   * Constructor for the coordinator class
   *
   * @param[in] instanceManager The instance manager backend to use
   * @param[in] communicationManager The communication manager backend to use
   * @param[in] memoryManager The memory manager backend to use
   */
  DistributedEngine(HiCR::L1::InstanceManager                *instanceManager,
           HiCR::L1::CommunicationManager           *communicationManager,
           HiCR::L1::MemoryManager                  *memoryManager)
    : _instanceManager(instanceManager),
      _communicationManager(communicationManager),
      _memoryManager(memoryManager)
  {}

  virtual ~DistributedEngine() = default;

  __INLINE__ void initialize()
  {
    ///// Finding memory space for buffer allocation

    // Creating HWloc topology object
    hwloc_topology_t topology;

    // Reserving memory for hwloc
    hwloc_topology_init(&topology);

    // Initializing host (CPU) topology manager
    HiCR::backend::host::hwloc::L1::TopologyManager tm(&topology);

    // Gathering topology from the topology manager
    const auto t = tm.queryTopology();

    // Selecting first device
    auto d = *t.getDevices().begin();

    // Getting memory space list from device
    auto memSpaces = d->getMemorySpaceList();

    // Grabbing first memory space for buffering
    _bufferMemorySpace = *memSpaces.begin();

    ///// Initializing channels
    initializeChannels();
  }

  /**
   * Asynchronously sends a binary message (buffer + size) to another instance
   *
   * @param[in] instanceId The id of the instance to which to send the message
   * @param[in] messagePtr The pointer to the message buffer
   * @param[in] messageSize The message size in bytes
   */
  __INLINE__ void sendMessage(const HiCR::L0::Instance::instanceId_t instanceId, void *messagePtr, size_t messageSize)
  {
    // Sanity check
    if (_producerChannels.contains(instanceId) == false) HICR_THROW_RUNTIME("DistributedEngine Id %lu not found in the producer channel map");

    // Getting reference to the appropriate producer channel
    const auto &channel = _producerChannels[instanceId];

    // Sending initial message to all workers
    auto messageSendSlot = _memoryManager->registerLocalMemorySlot(_bufferMemorySpace, messagePtr, messageSize);

    // Sending message
    channel->push(messageSendSlot);
  }

  /**
   * Synchronous function to receive a message from another instance
   *
   * @param[in] isAsync Whether the function must return immediately if no message was found
   * @return A pair containing a pointer to the start of the message binary data and the message's size
   */
  __INLINE__ DistributedEngine::message_t recvMessage(const bool isAsync)
  {
    // If asynchronous and consumerChannel is empty, return immediately
    if (isAsync == true && _consumerChannel->getDepth() == 0) return {NULL, 0};

    // Waiting for initial message from coordinator
    while (_consumerChannel->getDepth() == 0)
      ;

    // Get internal pointer of the token buffer slot and the offset
    auto payloadBufferMemorySlot = _consumerChannel->getPayloadBufferMemorySlot();
    auto payloadBufferPtr        = (const char *)payloadBufferMemorySlot->getSourceLocalMemorySlot()->getPointer();

    // Obtaining pointer from the offset + base pointer
    auto        offset = _consumerChannel->peek()[0];
    const void *ptr    = &payloadBufferPtr[offset];

    // Obtaining size
    const size_t size = _consumerChannel->peek()[1];

    // Popping message from the _consumerChannel
    _consumerChannel->pop();

    // Returning ptr + size pair
    return message_t{.data = ptr, .size = size};
  }

  /**
   * Asynchronous function to receive a message from another instance
   *
   * This function returns immediately.
   *
   * @return A pair containing a pointer to the start of the message binary data and the message's size. The pointer will be NULL if no messages were there when called.
   */
  __INLINE__ message_t recvMessageAsync() { return recvMessage(true); }

  /**
   * Function to initialize producer and consumer channels with all the rest of the instances
   */
  __INLINE__ void initializeChannels()
  {
    // Getting my current instance
    const auto currentInstanceId = _instanceManager->getCurrentInstance()->getId();

    ////////// Creating consumer channel to receive variable sized messages from other instances

    // Getting required buffer sizes
    auto tokenSizeBufferSize = HiCR::channel::variableSize::Base::getTokenBufferSize(sizeof(size_t), _HICR_DEPLOYER_CHANNEL_COUNT_CAPACITY);

    // Allocating token size buffer as a local memory slot
    auto tokenSizeBufferSlot = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, tokenSizeBufferSize);

    // Allocating token size buffer as a local memory slot
    auto payloadBufferSlot = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, _HICR_DEPLOYER_CHANNEL_PAYLOAD_CAPACITY);

    // Getting required buffer size for coordination buffers
    auto coordinationBufferSize = HiCR::channel::variableSize::Base::getCoordinationBufferSize();

    // Allocating coordination buffers
    auto localConsumerCoordinationBufferMessageSizes    = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, coordinationBufferSize);
    auto localConsumerCoordinationBufferMessagePayloads = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, coordinationBufferSize);

    // Initializing coordination buffers
    HiCR::channel::variableSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferMessageSizes);
    HiCR::channel::variableSize::Base::initializeCoordinationBuffer(localConsumerCoordinationBufferMessagePayloads);

    // Exchanging local memory slots to become global for them to be used by the remote end
    _communicationManager->exchangeGlobalMemorySlots(_HICR_DEPLOYER_CHANNEL_CONSUMER_SIZES_BUFFER_TAG, {{currentInstanceId, tokenSizeBufferSlot}});
    _communicationManager->fence(_HICR_DEPLOYER_CHANNEL_CONSUMER_SIZES_BUFFER_TAG);

    _communicationManager->exchangeGlobalMemorySlots(_HICR_DEPLOYER_CHANNEL_CONSUMER_PAYLOAD_BUFFER_TAG, {{currentInstanceId, payloadBufferSlot}});
    _communicationManager->fence(_HICR_DEPLOYER_CHANNEL_CONSUMER_PAYLOAD_BUFFER_TAG);

    _communicationManager->exchangeGlobalMemorySlots(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_SIZES_TAG,
                                                    {{currentInstanceId, localConsumerCoordinationBufferMessageSizes}});
    _communicationManager->fence(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_SIZES_TAG);

    _communicationManager->exchangeGlobalMemorySlots(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_PAYLOADS_TAG,
                                                    {{currentInstanceId, localConsumerCoordinationBufferMessagePayloads}});
    _communicationManager->fence(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_PAYLOADS_TAG);

    // Obtaining the globally exchanged memory slots
    auto consumerMessagePayloadBuffer      = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_PAYLOAD_BUFFER_TAG, currentInstanceId);
    auto consumerMessageSizesBuffer        = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_SIZES_BUFFER_TAG, currentInstanceId);
    auto consumerCoodinationPayloadsBuffer = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_PAYLOADS_TAG, currentInstanceId);
    auto consumerCoordinationSizesBuffer   = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_SIZES_TAG, currentInstanceId);

    // Creating channel
    _consumerChannel = std::make_shared<HiCR::channel::variableSize::MPSC::locking::Consumer>(*_communicationManager,
                                                                                              consumerMessagePayloadBuffer,
                                                                                              consumerMessageSizesBuffer,
                                                                                              localConsumerCoordinationBufferMessageSizes,
                                                                                              localConsumerCoordinationBufferMessagePayloads,
                                                                                              consumerCoordinationSizesBuffer,
                                                                                              consumerCoodinationPayloadsBuffer,
                                                                                              _HICR_DEPLOYER_CHANNEL_PAYLOAD_CAPACITY,
                                                                                              sizeof(uint8_t),
                                                                                              _HICR_DEPLOYER_CHANNEL_COUNT_CAPACITY);

    // Creating producer channels
    for (const auto &instance : _instanceManager->getInstances())
    {
      // Getting consumer instance id
      const auto consumerInstanceId = instance->getId();

      // Allocating coordination buffers
      auto localProducerSizeInfoBuffer                    = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, sizeof(size_t));
      auto localProducerCoordinationBufferMessageSizes    = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, coordinationBufferSize);
      auto localProducerCoordinationBufferMessagePayloads = _memoryManager->allocateLocalMemorySlot(_bufferMemorySpace, coordinationBufferSize);

      // Initializing coordination buffers
      HiCR::channel::variableSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferMessageSizes);
      HiCR::channel::variableSize::Base::initializeCoordinationBuffer(localProducerCoordinationBufferMessagePayloads);

      // Obtaining the globally exchanged memory slots
      auto consumerMessagePayloadBuffer  = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_PAYLOAD_BUFFER_TAG, consumerInstanceId);
      auto consumerMessageSizesBuffer    = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_SIZES_BUFFER_TAG, consumerInstanceId);
      auto consumerPayloadConsumerBuffer = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_PAYLOADS_TAG, consumerInstanceId);
      auto consumerSizesConsumerBuffer   = _communicationManager->getGlobalMemorySlot(_HICR_DEPLOYER_CHANNEL_CONSUMER_COORDINATION_BUFFER_SIZES_TAG, consumerInstanceId);

      // Creating channel
      _producerChannels[consumerInstanceId] = std::make_shared<HiCR::channel::variableSize::MPSC::locking::Producer>(*_communicationManager,
                                                                                                                    localProducerSizeInfoBuffer,
                                                                                                                    consumerMessagePayloadBuffer,
                                                                                                                    consumerMessageSizesBuffer,
                                                                                                                    localProducerCoordinationBufferMessageSizes,
                                                                                                                    localProducerCoordinationBufferMessagePayloads,
                                                                                                                    consumerSizesConsumerBuffer,
                                                                                                                    consumerPayloadConsumerBuffer,
                                                                                                                    _HICR_DEPLOYER_CHANNEL_PAYLOAD_CAPACITY,
                                                                                                                    sizeof(uint8_t),
                                                                                                                    _HICR_DEPLOYER_CHANNEL_COUNT_CAPACITY);
    }
  }

  /**
   * Function to finalize producer and consumer channels with all the rest of the instances
   */
  __INLINE__ void finalizeChannels() {}

  /**
  * Prompts the currently running instance to start listening for incoming RPCs
  */
  __INLINE__ void listen()
  {
    // Flag to indicate whether the worker should continue listening
    bool continueListening = true;

    // Adding RPC targets, specifying a name and the execution unit to execute
    _instanceManager->addRPCTarget("__finalize", [&]() { continueListening = false; });
    _instanceManager->addRPCTarget("__initializeChannels", [this]() { initializeChannels(); });

    // Listening for RPC requests
    while (continueListening == true) _instanceManager->listen();

    // Finalizing by sending a last acknowledgment as return value to the "__finalize" RPC
    uint8_t ack = 0;

    // Registering an empty return value to ack on finalization
    _instanceManager->submitReturnValue(&ack, sizeof(ack));

    // Finalizeing producer and consumer channels
    finalizeChannels();

    // Finalizing instance manager
    _instanceManager->finalize();

    // Exiting now
    exit(0);
  }

  protected:

  /**
   * Detected instance manager to use for detecting and creating HiCR instances (only one allowed)
   */
  HiCR::L1::InstanceManager *const _instanceManager = nullptr;

  /**
   * Detected communication manager to use for communicating between HiCR instances
   */
  HiCR::L1::CommunicationManager *const _communicationManager = nullptr;

  /**
   * Detected memory manager to use for allocating memory
   */
  HiCR::L1::MemoryManager *const _memoryManager;

  /**
   * Producer channels for sending messages to all other instances
   */
  std::map<HiCR::L0::Instance::instanceId_t, std::shared_ptr<ProducerChannel>> _producerChannels;

  /**
   * Consumer channels for receiving messages from all other instances
   */
  std::shared_ptr<ConsumerChannel> _consumerChannel;

  /**
   * Memory Space to use for buffer allocation
   */
  std::shared_ptr<HiCR::L0::MemorySpace> _bufferMemorySpace;
};

} // namespace taskr
