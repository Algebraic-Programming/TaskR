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

#include <memory>
#include <set>
#include <vector>

#include <hwloc.h>
#include <hicr/backends/hwloc/topologyManager.hpp>
#include <hicr/backends/boost/computeManager.hpp>
#include <hicr/backends/pthreads/computeManager.hpp>

#include <nosv.h>
#include <hicr/backends/nosv/common.hpp>
#include <hicr/backends/nosv/computeManager.hpp>

#include <taskr/taskr.hpp>

namespace taskr
{

enum backend_t
{
  /**
   * HiCR's nOS-V backend with the executionStates and ProcessingUnits being nOS-V
   */
  nosv,

  /**
   * executionStates are Boost and ProcessingUnits are Pthreads
   */
  threading
};


class PyRuntime
{
  public:

  /**
    * 
    */
  PyRuntime(const backend_t& backend_type = backend_t::nosv, size_t num_workers = 0)
    : _backend_type(backend_type)
  {
    // Specify the compute Managers
    if (_backend_type == backend_t::nosv)
    {
      // Initialize nosv
      check(nosv_init());

      // nosv task instance for the main thread
      nosv_task_t mainTask;

      // Attaching the main thread
      check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

      _executionStateComputeManager = std::make_unique<HiCR::backend::nosv::ComputeManager>();
      _processingUnitComputeManager = std::make_unique<HiCR::backend::nosv::ComputeManager>();
    }
    else if (_backend_type == backend_t::threading)
    {
      _executionStateComputeManager = std::make_unique<HiCR::backend::boost::ComputeManager>();
      _processingUnitComputeManager = std::make_unique<HiCR::backend::pthreads::ComputeManager>();
    }
    else { HICR_THROW_LOGIC("'%d' is not a known HiCR backend. Try 'nosv' or 'threading'\n", _backend_type); }

    // Reserving memory for hwloc
    hwloc_topology_init(&_topology);

    // Initializing HWLoc-based host (CPU) topology manager
    HiCR::backend::hwloc::TopologyManager tm(&_topology);

    // Asking backend to check the available devices
    const auto t = tm.queryTopology();

    // Compute resources to use
    HiCR::Device::computeResourceList_t _computeResources;

    // Getting compute resources in this device
    auto cr = (*(t.getDevices().begin()))->getComputeResourceList();

    auto itr = cr.begin();

    // Allocate the compute resources (i.e. PUs)
    if (num_workers == 0) { num_workers = cr.size(); }
    else if (num_workers > cr.size()) { HICR_THROW_LOGIC("num_workers = %d is not a legal number. FYI, we can have at most %d workers.\n", num_workers, cr.size()); }

    for (size_t i = 0; i < num_workers; i++)
    {
      _computeResources.push_back(*itr);
      itr++;
    }

    _num_workers = num_workers;

    _runtime = std::make_unique<Runtime>(_executionStateComputeManager.get(), _processingUnitComputeManager.get(), _computeResources);
  }

  /**
    * 
    */
  PyRuntime(const backend_t& backend_type, const std::set<int> &workersSet)
    : _backend_type(backend_type)
  {
    // Check if the workerSet is not empty
    if (workersSet.empty()) { HICR_THROW_LOGIC("Error: no compute resources provided\n"); }

    // Specify the compute Managers
    if (_backend_type == backend_t::nosv)
    {
      // Initialize nosv
      check(nosv_init());

      // nosv task instance for the main thread
      nosv_task_t mainTask;

      // Attaching the main thread
      check(nosv_attach(&mainTask, NULL, NULL, NOSV_ATTACH_NONE));

      _executionStateComputeManager = std::make_unique<HiCR::backend::nosv::ComputeManager>();
      _processingUnitComputeManager = std::make_unique<HiCR::backend::nosv::ComputeManager>();
    }
    else if (_backend_type == backend_t::threading)
    {
      _executionStateComputeManager = std::make_unique<HiCR::backend::boost::ComputeManager>();
      _processingUnitComputeManager = std::make_unique<HiCR::backend::pthreads::ComputeManager>();
    }
    else { HICR_THROW_LOGIC("'%d' is not a known HiCR backend. Try 'nosv' or 'threading'\n", _backend_type); }

    // Reserving memory for hwloc
    hwloc_topology_init(&_topology);

    // Initializing HWLoc-based host (CPU) topology manager
    HiCR::backend::hwloc::TopologyManager tm(&_topology);

    // Asking backend to check the available devices
    const auto t = tm.queryTopology();

    // Getting compute resource lists from devices
    std::vector<HiCR::Device::computeResourceList_t> computeResourceLists;
    for (auto d : t.getDevices()) computeResourceLists.push_back(d->getComputeResourceList());

    // Create processing units from the detected compute resource list and giving them to taskr
    HiCR::Device::computeResourceList_t _computeResources;
    for (auto computeResourceList : computeResourceLists)
      for (auto computeResource : computeResourceList)
      {
        // Interpreting compute resource as core
        auto core = dynamic_pointer_cast<HiCR::backend::hwloc::ComputeResource>(computeResource);

        // If the core affinity is included in the list, Add it to the list
        if (workersSet.contains(core->getProcessorId())) _computeResources.push_back(computeResource);
      }

    if (!_computeResources.size()) { HICR_THROW_LOGIC("Error: non-existing compute resources provided\n"); }

    _num_workers = _computeResources.size();

    _runtime = std::make_unique<Runtime>(_executionStateComputeManager.get(), _processingUnitComputeManager.get(), _computeResources);
  }

  /**
    * Destructor
    * Destroying topology and shutting down nOS-V if nosv backend have been used.
    */
  ~PyRuntime()
  {
    // Freeing up memory
    hwloc_topology_destroy(_topology);

    if (_backend_type == backend_t::nosv)
    {
      // Detaching the main thread
      check(nosv_detach(NOSV_DETACH_NONE));

      // Shutdown nosv
      check(nosv_shutdown());
    }
  }

  Runtime &get_runtime() { return *_runtime; }

  const size_t get_num_workers() { return _num_workers; }

  private:
  
  backend_t _backend_type;
  
  size_t _num_workers;
  
  std::unique_ptr<Runtime> _runtime;

  std::unique_ptr<HiCR::ComputeManager> _executionStateComputeManager;

  std::unique_ptr<HiCR::ComputeManager> _processingUnitComputeManager;

  hwloc_topology_t _topology;

  const HiCR::Device::computeResourceList_t _computeResources;
};

} // namespace taskr