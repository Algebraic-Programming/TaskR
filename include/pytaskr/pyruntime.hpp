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

class PyRuntime
{
public:
    PyRuntime(const std::string& str, size_t num_workers = 0) : _str(str)
    {
        // Specify the compute Managers
        if(_str == "nosv")
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
        else if(_str == "threading")
        {
            _executionStateComputeManager = std::make_unique<HiCR::backend::boost::ComputeManager>();
            _processingUnitComputeManager = std::make_unique<HiCR::backend::pthreads::ComputeManager>();
        }
        else
        {
            HICR_THROW_LOGIC("'%s' is not a known HiCR backend. Try 'nosv' or 'threading'\n", str);
        }

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

        // Specify allocate the compute resources (i.e. PUs)
        if(num_workers == 0)
        {
            num_workers = cr.size();
        }
        else if(num_workers >= 1 || num_workers <= cr.size())
        {
            // valid, do nothing
        }
        else
        {
            HICR_THROW_LOGIC("num_workers = %d is not a legal number. We can have at most %d workers.\n", num_workers, cr.size());
        }
        
        for (size_t i = 0; i < num_workers; i++)
        {
            _computeResources.push_back(*itr);
            itr++;
        }

        _runtime = std::make_unique<Runtime>(_executionStateComputeManager.get(), _processingUnitComputeManager.get(), _computeResources);
    }

    ~PyRuntime()
    {
        // Freeing up memory
        hwloc_topology_destroy(_topology);

        if(_str == "nosv")
        {
            // Detaching the main thread
            check(nosv_detach(NOSV_DETACH_NONE));

            // Shutdown nosv
            check(nosv_shutdown());
        }
    }
    
    Runtime* get_runtime()
    {
        return _runtime.get();
    }
    
    private:

    const std::string _str;

    std::unique_ptr<HiCR::ComputeManager> _executionStateComputeManager;
    
    std::unique_ptr<HiCR::ComputeManager> _processingUnitComputeManager;
    
    hwloc_topology_t _topology;
    
    const HiCR::Device::computeResourceList_t _computeResources;
    
    std::unique_ptr<Runtime> _runtime;
};

} // namespace taskr