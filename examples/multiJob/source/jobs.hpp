#include <cstdio>
#include <hicr/core/L0/device.hpp>
#include <hicr/backends/host/L1/computeManager.hpp>
#include <taskr/taskr.hpp>

#define ITERATIONS 100

void job1(HiCR::backend::host::L1::ComputeManager *computeManager, taskr::Runtime &taskr);
void job2(HiCR::backend::host::L1::ComputeManager *computeManager, taskr::Runtime &taskr);