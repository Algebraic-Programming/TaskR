#include <mpi.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>
#include "grid.hpp"

// Singleton Grid


int main (int carg, char* argv[])
{
 int processId = 0;
 int processCount = 1;

 MPI_Init(&carg, &argv);
 MPI_Comm_rank(MPI_COMM_WORLD, &processId);
 MPI_Comm_size(MPI_COMM_WORLD, &processCount);

 // Is root rank (prints messages)
 bool isRootRank = processId == 0;

 // Setting default values
 size_t gDepth = 1;
 size_t N = 128;
 size_t nIters=100;
 D3 pt = D3({.x = 1, .y = 1, .z = 1});

 // Parsing user inputs
 for (int i = 0; i < carg; i++)
 {
  if(!strcmp(argv[i], "-px")) pt.x = atoi(argv[++i]);
  if(!strcmp(argv[i], "-py")) pt.y = atoi(argv[++i]);
  if(!strcmp(argv[i], "-pz")) pt.z = atoi(argv[++i]);
  if(!strcmp(argv[i], "-n"))  N  = atoi(argv[++i]);
  if(!strcmp(argv[i], "-i"))  nIters = atoi(argv[++i]);
 }

 if (pt.x * pt.y * pt.z != processCount) { if (isRootRank) printf("[Error] The specified px/py/pz geometry does not match the number of MPI processes (-n %d).\n", processCount); MPI_Abort(MPI_COMM_WORLD, 1); }

 // Initializing grid
 Grid g(processId, N, nIters, gDepth, pt);
 bool success = g.initialize();
 if (success == false) MPI_Abort(MPI_COMM_WORLD, 1);

 // Resetting grid
 g.reset();

 // Solving (and timing)
 MPI_Barrier(MPI_COMM_WORLD);
 double execTime = -MPI_Wtime();
 g.solve();
 MPI_Barrier(MPI_COMM_WORLD);

 // Calculating residual
 double residual = g.calculateResidual(nIters);
 //printf("Process: %d, Residual: %.8f\n", processId, g._localResidual);
 execTime += MPI_Wtime();

//  for (size_t i = 0; i < processCount; i++)
//  {
//     if (processId == i) 
//     {
//         printf("Process: %d, Residual: %.8f\n", processId, g._localResidual);
//         g.print(nIters);
//     }
//     printf("\n");

//     usleep(50000);
//  }

 if (isRootRank)
 {
  double gflops = nIters*(double)N*(double)N*(double)N*(2 + gDepth * 8)/(1.0e9);
  printf("%.4fs, %.3f GFlop/s (L2 Norm: %.10g)\n", execTime, gflops/execTime, residual);
 }

 // Finalizing grid
 MPI_Barrier(MPI_COMM_WORLD);
 g.finalize();

 MPI_Finalize();
 return 0;
}

