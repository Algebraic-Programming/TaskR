#pragma once

#include <lapack.h>

/**
 * Initialize a symmetric matrix with random generated values
 *
 * @param[in] matrix the pointer the the matrix memory
 * @param[in] dimension size of the matrix dimension
*/
void initMatrix(double *__restrict__ matrix, uint32_t dimension)
{
  int n        = dimension;
  int ISEED[4] = {0, 0, 0, 1};
  int intONE   = 1;

// Get a vector of randomized numbers
#pragma omp parallel for
  for (int i = 0; i < n * n; i += n) { dlarnv_(&intONE, &ISEED[0], &n, &matrix[i]); }

#pragma omp parallel for
  for (int i = 0; i < n; i++)
  {
    for (int j = 0; j <= i; j++)
    {
      // Make the matrix simmetrical
      matrix[j * n + i] = matrix[j * n + i] + matrix[i * n + j];
      matrix[i * n + j] = matrix[j * n + i];
    }
  }

// Increase the diagonal by N
#pragma omp parallel for
  for (int i = 0; i < n; i++) matrix[i + i * n] += (double)n;
}
