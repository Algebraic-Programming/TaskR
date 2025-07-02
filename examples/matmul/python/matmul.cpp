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

#include <chrono>

#include <taskr/taskr.hpp>
#include <pytaskr/pytaskr.hpp>

#define mytype float

/**
 * Compute mmm
 */
void matmul(taskr::Task *)
{
  const size_t N = 1000;

  // Allocate memory
  volatile mytype *A = (mytype *)calloc(1, N * N * sizeof(mytype));
  volatile mytype *B = (mytype *)malloc(N * N * sizeof(mytype));
  volatile mytype *C = (mytype *)malloc(N * N * sizeof(mytype));

  // Filling matrices B and C
  for (size_t i = 0; i < N; ++i)
  {
    for (size_t j = 0; j < N; ++j)
    {
      B[i * N + j] = 1.0 / (mytype(i + 1));
      C[i * N + j] = 1.0 / (mytype(j + 1));
    }
  }

  // mmm
  for (size_t i = 0; i < N; ++i)
  {
    for (size_t j = 0; j < N; ++j)
    {
      for (size_t k = 0; k < N; ++k) { A[i * N + j] += B[i * N + k] * C[k * N + j]; }
    }
  }

  // free memory
  free((mytype *)A);
  free((mytype *)B);
  free((mytype *)C);
}

namespace taskr
{

__attribute__((constructor))  // GCC/Clang: Run before main/init
void register_my_func()
{
  register_function("cpp_matmul", matmul);
}

} // namespace taskr