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
#include <pybind11/pybind11.h>

#include <taskr/taskr.hpp>

#ifdef ENABLE_INSTRUMENTATION
  #include <tracr.hpp>
#endif

#define mytype float

/**
 * Compute mmm
 */
void matmul(taskr::Task *)
{
#ifdef ENABLE_INSTRUMENTATION
  INSTRUMENTATION_VMARKER_SET(MARK_COLOR_RED);
#endif

  const size_t N = 200;

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

#ifdef ENABLE_INSTRUMENTATION
  INSTRUMENTATION_VMARKER_RESET();
#endif
}

PYBIND11_MODULE(cpp_matmul, m)
{
  m.doc() = "pybind11 plugin for matmul example";

  m.def("cpp_matmul", &matmul, "cpp function to do matrix-matrix multiplication.");
}