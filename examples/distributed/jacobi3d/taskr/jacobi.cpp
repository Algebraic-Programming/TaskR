#include <mpi.h>
#include <string.h>
#include <stdio.h>
#include <taskr.hpp>
#include <stdlib.h>
#include <math.h>
#include "grid.hpp"

int main (int argc, char* argv[])
{
 // Initializing MPI with support for multiple thread access
 int provided, requested = MPI_THREAD_MULTIPLE;
 MPI_Init_thread(&argc, &argv, requested, &provided);
 if (provided != requested) { fprintf(stderr, "MPI Init Error. Level required: %d, Level provided: %d\n", requested, provided); exit(-1); }

 // Getting process distribution info
 int processId = 0;
 int processCount = 1;
 MPI_Comm_size(MPI_COMM_WORLD, &processCount);
 MPI_Comm_rank(MPI_COMM_WORLD, &processId);

 // Is root rank (prints messages)
 bool isRootRank = processId == 0;

 // Setting default values
 size_t gDepth = 2;
 size_t N = 128;
 size_t nIters=100;
 D3 pt = D3({.x = 1, .y = 1, .z = 1});
 D3 lt = D3({.x = 1, .y = 1, .z = 1});

 // Parsing user inputs
 for (int i = 0; i < argc; i++)
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

 if (pt.x * pt.y * pt.z != processCount) { if (isRootRank) printf("[Error] The specified px/py/pz geometry does not match the number of MPI processes (-n %d).\n", processCount); MPI_Abort(MPI_COMM_WORLD, 1); }

 // Creating and initializing Grid
 auto g = new Grid(processId, N, nIters, gDepth, pt, lt);
 bool success = g->initialize();
 if (success == false) MPI_Abort(MPI_COMM_WORLD, 1);

 // Initializing taskr
 taskr::initialize();
 //taskr::enableDebugInfo();

 // Creating tasks to reset the grid
 for (size_t i = 0; i < lt.x; i++)
  for (size_t j = 0; j < lt.y; j++)
   for (size_t k = 0; k < lt.z; k++)
   {
    auto task = taskr::Task(Grid::encodeTaskName("Reset", i, j, k, 0));
    taskr::addTask(task, [g, i, j, k](){ g->reset(i, j, k); });
   }

 //  Running initialization
 taskr::run();

 // Creating initial set tasks to solve the first iteration
 for (size_t i = 0; i < lt.x; i++)
  for (size_t j = 0; j < lt.y; j++)
   for (size_t k = 0; k < lt.z; k++)
   {
    if (nIters > 0)
    {
     auto computeTask = taskr::Task(Grid::encodeTaskName("Compute", i, j, k, 0));
     taskr::addTask(computeTask, [g, i, j, k](){ g->compute(i, j, k, 0); });
    }

    if (nIters > 1)
    {
     auto packTask = taskr::Task(Grid::encodeTaskName("Pack", i, j, k, 0));
     packTask.addTaskDependency(Grid::encodeTaskName("Compute", i, j, k, 0));
     taskr::addTask(packTask, [g, i, j, k](){ g->pack(i, j, k, 0); });

     auto sendTask = taskr::Task(Grid::encodeTaskName("Send", i, j, k, 0));
     sendTask.addTaskDependency(Grid::encodeTaskName("Pack", i, j, k, 0));
     taskr::addTask(sendTask, [g, i, j, k](){ g->send(i, j, k, 0); });

     auto recvTask = taskr::Task(Grid::encodeTaskName("Receive", i, j, k, 0));
     taskr::addTask(recvTask, [g, i, j, k](){ g->receive(i, j, k, 0); });

     auto unpackTask = taskr::Task(Grid::encodeTaskName("Unpack", i, j, k, 0));
     unpackTask.addTaskDependency(Grid::encodeTaskName("Compute", i, j, k, 0));
     unpackTask.addTaskDependency(Grid::encodeTaskName("Receive", i, j, k, 0));
     taskr::addTask(unpackTask, [g, i, j, k](){ g->unpack(i, j, k, 0); });
    }
   }

 // Solving (and timing)
 MPI_Barrier(MPI_COMM_WORLD);
 double execTime = -MPI_Wtime();

 // Running solver tasks
 taskr::run();

 // Re-timing at the end
 MPI_Barrier(MPI_COMM_WORLD);
 execTime += MPI_Wtime();

 // Creating tasks to calculate the residual
 for (size_t i = 0; i < lt.x; i++)
  for (size_t j = 0; j < lt.y; j++)
   for (size_t k = 0; k < lt.z; k++)
   {
    auto task = taskr::Task("Residual");
    taskr::addTask(task, [g, i, j, k, nIters](){ g->calculateResidual(i, j, k, nIters); });
   }

 // Calculating residual
 g->_residual = 0.0;
 taskr::run();

 double globalRes = 0.0;
 MPI_Reduce (&g->_residual, &globalRes, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
 g->_residual = sqrt(globalRes/((double)(N-1)*(double)(N-1)*(double)(N-1)));
 double gflops = nIters*(double)N*(double)N*(double)N*(2 + gDepth * 8)/(1.0e9);
 if (isRootRank) printf("%.4fs, %.3f GFlop/s (L2 Norm: %.10g)\n", execTime, gflops/execTime, g->_residual);

 // Printing for debug purposes
 //g->print(nIters);

 // Finalizing grid
 MPI_Barrier(MPI_COMM_WORLD);
 g->finalize();

 taskr::finalize();
 MPI_Finalize();
 return 0;
}

