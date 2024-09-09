#include <mpi.h>
#include <string.h>
#include <stdio.h>
#include "grid.hpp"

// Singleton Grid
Grid* g;

int main (int carg, char* argv[])
{
 int processId = 0;
 int localId = 0;
 int processCount = 1;
 int localRankCount = 1;

 Mate_Init(&carg, &argv);
 Mate_global_process_id(&processId);
 Mate_global_process_count(&processCount);
 Mate_local_rank_count(&localRankCount);
 Mate_local_rank_id(&localId);

 // Is root rank (prints messages)
 bool isRootRank = processId == 0 && localId == 0;

 // Setting default values
 size_t gDepth = 2;
 size_t N = 128;
 size_t nIters=100;
 D3 pt = D3({.x = 1, .y = 1, .z = 1});
 D3 lt = D3({.x = 1, .y = 1, .z = 1});

 // Parsing user inputs
 for (int i = 0; i < carg; i++)
 {
  if(!strcmp(argv[i], "-px")) pt.x = atoi(argv[++i]);
  if(!strcmp(argv[i], "-py")) pt.y = atoi(argv[++i]);
  if(!strcmp(argv[i], "-pz")) pt.z = atoi(argv[++i]);
  if(!strcmp(argv[i], "-lx")) lt.x = atoi(argv[++i]);
  if(!strcmp(argv[i], "-ly")) lt.y = atoi(argv[++i]);
  if(!strcmp(argv[i], "-lz")) lt.z = atoi(argv[++i]);
  if(!strcmp(argv[i], "-n"))  N  = atoi(argv[++i]);
  if(!strcmp(argv[i], "-i"))  nIters = atoi(argv[++i]);
 }

 if (pt.x * pt.y * pt.z != processCount) { if (isRootRank) printf("[Error] The specified px/py/pz geometry does not match the number of MATE processes (-n %d).\n", processCount); MPI_Abort(MPI_COMM_WORLD, 1); }
 if (lt.x * lt.y * lt.z != localRankCount) { if (isRootRank) printf("[Error] The specified lx/ly/lz geometry does not match the number of local ranks (--mate-ranks %d).\n", localRankCount); MPI_Abort(MPI_COMM_WORLD, 1); }

 // Initializing grid
 if (localId == 0)
 {
  g = new Grid(processId, N, nIters, gDepth, pt, lt);
  bool success = g->initialize();
  if (success == false) MPI_Abort(MPI_COMM_WORLD, 1);
 }

 // Resetting grid
 Mate_LocalBarrier();
 g->reset();
 Mate_LocalBarrier();

 // Solving (and timing)
 MPI_Barrier(MPI_COMM_WORLD);
 double execTime = -Mate_Wtime();
 g->solve();
 MPI_Barrier(MPI_COMM_WORLD);
 execTime += Mate_Wtime();

 // Calculating residual
 double residual = g->calculateResidual();
 if (isRootRank)
 {
  double gflops = nIters*(double)N*(double)N*(double)N*(2 + gDepth * 8)/(1.0e9);
  printf("%.4fs, %.3f GFlop/s (L2 Norm: %.10g)\n", execTime, gflops/execTime, residual);
 }

 // Finalizing grid
 MPI_Barrier(MPI_COMM_WORLD);
 if (localId == 0) g->finalize();

 Mate_Finalize();
 return 0;
}

