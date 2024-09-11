#pragma once

#include <taskr/taskr.hpp>

// Jacobi Task, wraps a taskr task with metadata about the task's coordinates
class Task final : public taskr::Task
{
   public:

   Task(std::string taskType, ssize_t i, ssize_t j, ssize_t k, ssize_t iteration, std::shared_ptr<HiCR::L0::ExecutionUnit> executionUnit) : 
   taskr::Task(encodeTaskName(taskType, i, j, k, iteration), executionUnit),
   i(i), j(j), k(k), iteration(iteration) {}

   ~Task() = default;

   // X coordinate (index i);
   const ssize_t i;

   // Y coordinate (index j);
   const ssize_t j;

   // Z coordinate (index k);
   const ssize_t k;

   // Iteration this task corresponds to
   const ssize_t iteration;

   // Function to encode a task name into a unique identifier
   static inline size_t encodeTaskName(const std::string& taskName, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint64_t iter)
   {
     char buffer[512];
     sprintf(buffer, "%s_%lu_%lu_%lu_%lu", taskName.c_str(), lx, ly, lz, iter);
     const std::hash<std::string> hasher;
     const auto hashResult = hasher(buffer);
     return hashResult;
   }
};