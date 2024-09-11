#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include "grid.hpp"
#include "task.hpp"

Grid::Grid(taskr::Runtime* const taskr, const int processId, const size_t N, const size_t nIters, const size_t gDepth,const D3& pt,const D3& lt) : _taskr(taskr)
{
 // Copying values
 this->processId = processId;
 this->N = N;
 this->nIters = nIters;
 this->gDepth = gDepth;
 this->pt = pt;
 this->lt = lt;
}

bool Grid::initialize()
{
 // Inverse stencil coefficient
 invCoeff = 1.0 / (1.0 + 6.0 * (double)gDepth);

 // Checking topology correctness
 if(N % pt.x > 0) { fprintf(stderr, "Error: N (%lu) should be divisible by px (%lu)\n", N, pt.x); return false; }
 if(N % pt.y > 0) { fprintf(stderr, "Error: N (%lu) should be divisible by py (%lu)\n", N, pt.y); return false; }
 if(N % pt.z > 0) { fprintf(stderr, "Error: N (%lu) should be divisible by pz (%lu)\n", N, pt.z); return false; }

 // Calculating grid size per process
 this->ps.x = N / pt.x;
 this->ps.y = N / pt.y;
 this->ps.z = N / pt.z;

 if(ps.x % lt.x > 0) { fprintf(stderr, "Error: nx (%lu) should be divisible by lx (%lu)\n", ps.x, lt.x); return false; }
 if(ps.y % lt.y > 0) { fprintf(stderr, "Error: ny (%lu) should be divisible by ly (%lu)\n", ps.y, lt.y); return false; }
 if(ps.z % lt.z > 0) { fprintf(stderr, "Error: nz (%lu) should be divisible by lz (%lu)\n", ps.z, lt.z); return false; }

 // Calculating grid size per process plus ghost cells
 fs.x = ps.x + 2 * gDepth;
 fs.y = ps.y + 2 * gDepth;
 fs.z = ps.z + 2 * gDepth;

 U  = (double *)malloc(sizeof(double)*fs.x*fs.y*fs.z);
 Un = (double *)malloc(sizeof(double)*fs.x*fs.y*fs.z);

 size_t processCount = pt.x * pt.y * pt.z;
 ssize_t* globalRankX = (ssize_t*) calloc(sizeof(ssize_t),processCount);
 ssize_t* globalRankY = (ssize_t*) calloc(sizeof(ssize_t),processCount);
 ssize_t* globalRankZ = (ssize_t*) calloc(sizeof(ssize_t),processCount);

 globalProcessMapping = (ssize_t***) calloc(sizeof(ssize_t**),pt.z);
 for (ssize_t i = 0; i < pt.z; i++) globalProcessMapping[i] = (ssize_t**) calloc (sizeof(ssize_t*),pt.y);
 for (ssize_t i = 0; i < pt.z; i++) for (ssize_t j = 0; j < pt.y; j++) globalProcessMapping[i][j] = (ssize_t*) calloc (sizeof(ssize_t),pt.x);

 ssize_t currentRank = 0;
 for (ssize_t z = 0; z < pt.z; z++)
 for (ssize_t y = 0; y < pt.y; y++)
 for (ssize_t x = 0; x < pt.x; x++)
 { globalRankZ[currentRank] = z; globalRankX[currentRank] = x; globalRankY[currentRank] = y; globalProcessMapping[z][y][x] = currentRank; currentRank++; }

 //  if (processId == 0) for (int i = 0; i < processCount; i++) printf("Rank %d - Z: %d, Y: %d, X: %d\n", i, globalRankZ[i], globalRankY[i], globalRankX[i]);

 int curLocalRank = 0;
 localSubGridMapping = (ssize_t***) calloc (sizeof(ssize_t**), lt.z);
 for (ssize_t i = 0; i < lt.z; i++) localSubGridMapping[i] = (ssize_t**) calloc (sizeof(ssize_t*) , lt.y);
 for (ssize_t i = 0; i < lt.z; i++) for (ssize_t j = 0; j < lt.y; j++) localSubGridMapping[i][j] = (ssize_t*) calloc (sizeof(ssize_t) , lt.x);
 for (ssize_t i = 0; i < lt.z; i++) for (ssize_t j = 0; j < lt.y; j++) for (ssize_t k = 0; k < lt.x; k++) localSubGridMapping[i][j][k] = curLocalRank++;

 // Getting process-wise mapping
 pPos.z = globalRankZ[processId];
 pPos.y = globalRankY[processId];
 pPos.x = globalRankX[processId];

 // Grid size for local tasks
 ls.x = ps.x / lt.x;
 ls.y = ps.y / lt.y;
 ls.z = ps.z / lt.z;

 faceSizeX = ls.y*ls.z;
 faceSizeY = ls.x*ls.z;
 faceSizeZ = ls.x*ls.y;

 bufferSizeX = faceSizeX*gDepth;
 bufferSizeY = faceSizeY*gDepth;
 bufferSizeZ = faceSizeZ*gDepth;

 // Mapping for local tasks
 localRankCount = lt.x * lt.y * lt.z;
 subgrids.resize(localRankCount);
 for (ssize_t localId = 0; localId < localRankCount; localId++)
 {
  auto& t = subgrids[localId];

   // Processing local task mapping
  for (int i = 0; i < lt.z; i++)
  for (int j = 0; j < lt.y; j++)
  for (int k = 0; k < lt.x; k++)
   if (localSubGridMapping[i][j][k] == localId) {  t.lPos.z = i;  t.lPos.y = j; t.lPos.x = k; }

  t.lStart.x = ls.x * t.lPos.x + gDepth;
  t.lStart.y = ls.y * t.lPos.y + gDepth;
  t.lStart.z = ls.z * t.lPos.z + gDepth;
  t.lEnd.x = t.lStart.x + ls.x;
  t.lEnd.y = t.lStart.y + ls.y;
  t.lEnd.z = t.lStart.z + ls.z;

  t.West.type  = LOCAL; t.West.processId  = processId;
  t.East.type  = LOCAL; t.East.processId  = processId;
  t.North.type = LOCAL; t.North.processId = processId;
  t.South.type = LOCAL; t.South.processId = processId;
  t.Up.type    = LOCAL; t.Up.processId    = processId;
  t.Down.type  = LOCAL; t.Down.processId  = processId;

  t.West.lPos.x  = t.lPos.x - 1; t.West.lPos.y  = t.lPos.y;     t.West.lPos.z  = t.lPos.z;
  t.East.lPos.x  = t.lPos.x + 1; t.East.lPos.y  = t.lPos.y;     t.East.lPos.z  = t.lPos.z;
  t.North.lPos.x = t.lPos.x;     t.North.lPos.y = t.lPos.y - 1; t.North.lPos.z = t.lPos.z;
  t.South.lPos.x = t.lPos.x;     t.South.lPos.y = t.lPos.y + 1; t.South.lPos.z = t.lPos.z;
  t.Up.lPos.x    = t.lPos.x;     t.Up.lPos.y    = t.lPos.y;     t.Up.lPos.z    = t.lPos.z - 1;
  t.Down.lPos.x  = t.lPos.x;     t.Down.lPos.y  = t.lPos.y;     t.Down.lPos.z  = t.lPos.z + 1;

  if (t.West.lPos.x  == -1)   { t.West.type  = REMOTE;  t.West.lPos.x = lt.x-1; }
  if (t.East.lPos.x  == lt.x) { t.East.type  = REMOTE;  t.East.lPos.x = 0;    }
  if (t.North.lPos.y == -1)   { t.North.type = REMOTE; t.North.lPos.y = lt.y-1; }
  if (t.South.lPos.y == lt.y) { t.South.type = REMOTE; t.South.lPos.y = 0;    }
  if (t.Up.lPos.z    == -1)   { t.Up.type    = REMOTE;    t.Up.lPos.z = lt.z-1; }
  if (t.Down.lPos.z  == lt.z) { t.Down.type  = REMOTE;  t.Down.lPos.z = 0;    }

  if (pPos.x == 0    && t.lPos.x == 0)        t.West.type  = BOUNDARY;
  if (pPos.y == 0    && t.lPos.y == 0)        t.North.type = BOUNDARY;
  if (pPos.z == 0    && t.lPos.z == 0)        t.Up.type    = BOUNDARY;
  if (pPos.x == pt.x-1 && t.lPos.x == lt.x-1) t.East.type  = BOUNDARY;
  if (pPos.y == pt.y-1 && t.lPos.y == lt.y-1) t.South.type = BOUNDARY;
  if (pPos.z == pt.z-1 && t.lPos.z == lt.z-1) t.Down.type  = BOUNDARY;

  t.West.localId  = localSubGridMapping[t.lPos.z][t.lPos.y][t.West.lPos.x];
  t.East.localId  = localSubGridMapping[t.lPos.z][t.lPos.y][t.East.lPos.x];
  t.North.localId = localSubGridMapping[t.lPos.z][t.North.lPos.y][t.lPos.x];
  t.South.localId = localSubGridMapping[t.lPos.z][t.South.lPos.y][t.lPos.x];
  t.Up.localId    = localSubGridMapping[t.Up.lPos.z][t.lPos.y][t.lPos.x];
  t.Down.localId  = localSubGridMapping[t.Down.lPos.z][t.lPos.y][t.lPos.x];

  if (t.West.type  == REMOTE) t.West.processId  = globalProcessMapping[pPos.z][pPos.y][pPos.x-1];
  if (t.East.type  == REMOTE) t.East.processId  = globalProcessMapping[pPos.z][pPos.y][pPos.x+1];
  if (t.North.type == REMOTE) t.North.processId = globalProcessMapping[pPos.z][pPos.y-1][pPos.x];
  if (t.South.type == REMOTE) t.South.processId = globalProcessMapping[pPos.z][pPos.y+1][pPos.x];
  if (t.Up.type    == REMOTE) t.Up.processId    = globalProcessMapping[pPos.z-1][pPos.y][pPos.x];
  if (t.Down.type  == REMOTE) t.Down.processId  = globalProcessMapping[pPos.z+1][pPos.y][pPos.x];

  //////// CREATE CHANNELS HERE
 }

 free(globalRankX);
 free(globalRankY);
 free(globalRankZ);

 return true;
}

void Grid::finalize()
{
 free(U);
 free(Un);

 for (ssize_t i = 0; i < pt.z; i++) for (ssize_t j = 0; j < pt.y; j++) free(globalProcessMapping[i][j]);
 for (ssize_t i = 0; i < pt.z; i++) free(globalProcessMapping[i]);
 free(globalProcessMapping);

 for (ssize_t i = 0; i < lt.z; i++) for (ssize_t j = 0; j < lt.y; j++) free(localSubGridMapping[i][j]);
 for (ssize_t i = 0; i < lt.z; i++) free(localSubGridMapping[i]);
 free(localSubGridMapping);
}

void Grid::reset(const uint64_t lx, const uint64_t ly, const uint64_t lz)
{
 auto& t = subgrids[localSubGridMapping[lz][ly][lx]];

 for (int k = t.lStart.z-gDepth; k < t.lEnd.z+gDepth; k++)
 for (int j = t.lStart.y-gDepth; j < t.lEnd.y+gDepth; j++)
 for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++)
 {
   Un[k*fs.y*fs.x + j*fs.x + i] = 0.0;
   U[k*fs.y*fs.x + j*fs.x + i]  = 0.0;
 }

 if (t.West.type  == BOUNDARY) for (int i = t.lStart.y-gDepth; i < t.lEnd.y+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + i*fs.x + d] = 1.0;
 if (t.North.type == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + d*fs.x + i] = 1.0;
 if (t.Up.type    == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.y-gDepth; j < t.lEnd.y+gDepth; j++) for (int d = 0; d < gDepth; d++) U[d*fs.x*fs.y + j*fs.x + i] = 1.0;
 if (t.East.type  == BOUNDARY) for (int i = t.lStart.y-gDepth; i < t.lEnd.y+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + i*fs.x + (ps.x+gDepth+d)] = 1.0;
 if (t.South.type == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + (ps.y+gDepth+d)*fs.x + i] = 1.0;
 if (t.Down.type  == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.y-gDepth; j < t.lEnd.y+gDepth; j++) for (int d = 0; d < gDepth; d++) U[(ps.z+gDepth+d)*fs.x*fs.y + j*fs.x + i] = 1.0;
}

void Grid::compute(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
 auto localId = localSubGridMapping[lz][ly][lx];
 auto& subGrid = subgrids[localId];

 printf("Running Compute (%lu, %lu, %lu, %u / %lu). Subgrid: ([%lu %lu], [%lu %lu], [%lu %lu])\n", lx, ly, lz, it, nIters, subGrid.lStart.x, subGrid.lEnd.x, subGrid.lStart.y, subGrid.lEnd.y, subGrid.lStart.z, subGrid.lEnd.z);

 // Local pointer copies
 double *localU  = it % 2 == 0 ? U :  Un;
 double *localUn = it % 2 == 0 ? Un : U;

// printf("Rank %u running Compute (%lu, %lu, %lu, It: %u)\n", localId, lx, ly, lz, it); fflush(stdout);

 for (int k0 = subGrid.lStart.z; k0 < subGrid.lEnd.z; k0 += BLOCKZ)
 {
  int k1= k0 + BLOCKZ < subGrid.lEnd.z ? k0 + BLOCKZ : subGrid.lEnd.z;
  for (int j0 = subGrid.lStart.y; j0 < subGrid.lEnd.y; j0 += BLOCKY)
  {
   int j1= j0 + BLOCKY < subGrid.lEnd.y ? j0 + BLOCKY : subGrid.lEnd.y;
   for (int k = k0; k < k1; k++)
   {
    for (int j = j0; j < j1; j++)
    {
     #pragma GCC ivdep
     for (int i = subGrid.lStart.x; i < subGrid.lEnd.x; i++)
     {
      double sum = localU[fs.x*fs.y*k + fs.x*j + i]; // Central
      for (int d = 1; d <= gDepth; d++)
      {
       sum += localU[fs.x*fs.y*(k-d) + fs.x*j         + i]; // Up
       sum += localU[fs.x*fs.y*(k+d) + fs.x*j         + i]; // Down
       sum += localU[fs.x*fs.y*k     + fs.x*j     - d + i]; // East
       sum += localU[fs.x*fs.y*k     + fs.x*j     + d + i]; // West
       sum += localU[fs.x*fs.y*k     + fs.x*(j+d)     + i]; // North
       sum += localU[fs.x*fs.y*k     + fs.x*(j-d)     + i]; // South
      }
      localUn[fs.x*fs.y*k + fs.x*j + i] = sum * invCoeff; // Update
     }
    }
   }
  }
 }

 // If we reached the end, simply return and finish
 if (it + 1 == nIters) return;

 // Creating new task for the next iteration
 auto newTask = new Task("Compute", lx, ly, lz, it+1, computeFc);

 // Adding compute dependencies for the next iteration
 if(subGrid.West.type  == LOCAL) newTask->addDependency(Task::encodeTaskName("Compute", lx-1, ly+0, lz+0, it));
 if(subGrid.East.type  == LOCAL) newTask->addDependency(Task::encodeTaskName("Compute", lx+1, ly+0, lz+0, it));
 if(subGrid.North.type == LOCAL) newTask->addDependency(Task::encodeTaskName("Compute", lx+0, ly-1, lz+0, it));
 if(subGrid.South.type == LOCAL) newTask->addDependency(Task::encodeTaskName("Compute", lx+0, ly+1, lz+0, it));
 if(subGrid.Up.type    == LOCAL) newTask->addDependency(Task::encodeTaskName("Compute", lx+0, ly+0, lz-1, it));
 if(subGrid.Down.type  == LOCAL) newTask->addDependency(Task::encodeTaskName("Compute", lx+0, ly+0, lz+1, it));

 // Adding communication dependency for the next iteration
 newTask->addDependency(Task::encodeTaskName("Unpack", lx, ly, lz, it));

 // Adding task for the next iteration
 _taskr->addTask(newTask);
}

void Grid::receive(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
 auto localId = localSubGridMapping[lz][ly][lx];
 auto& subGrid = subgrids[localId];

//  if(subGrid.Down.type  == REMOTE) MPI_Irecv(subGrid.downRecvBuffer[it],  bufferSizeZ, MPI_DOUBLE, subGrid.Down.processId,  subGrid.Down.localId,  MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.Up.type    == REMOTE) MPI_Irecv(subGrid.upRecvBuffer[it],    bufferSizeZ, MPI_DOUBLE, subGrid.Up.processId,    subGrid.Up.localId,    MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.East.type  == REMOTE) MPI_Irecv(subGrid.eastRecvBuffer[it],  bufferSizeX, MPI_DOUBLE, subGrid.East.processId,  subGrid.East.localId,  MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.West.type  == REMOTE) MPI_Irecv(subGrid.westRecvBuffer[it],  bufferSizeX, MPI_DOUBLE, subGrid.West.processId,  subGrid.West.localId,  MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.North.type == REMOTE) MPI_Irecv(subGrid.northRecvBuffer[it], bufferSizeY, MPI_DOUBLE, subGrid.North.processId, subGrid.North.localId, MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.South.type == REMOTE) MPI_Irecv(subGrid.southRecvBuffer[it], bufferSizeY, MPI_DOUBLE, subGrid.South.processId, subGrid.South.localId, MPI_COMM_WORLD, &requests[reqIdx++]);

 // If we reached the iteration before the end, no more communication is needed
 if (it + 1 == nIters - 1) return;

 // Creating new task for the next iteration
 auto newTask = new Task("Receive", lx, ly, lz, it+1, receiveFc);

 // Creating task for the next iteration only if we haven't reached the end
 _taskr->addTask(newTask);
}

void Grid::unpack(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
 auto localId = localSubGridMapping[lz][ly][lx];
 auto& subGrid = subgrids[localId];

 // Local pointer copies
 double *localU  = it % 2 == 0 ? Un :  U;

 // printf("Rank %u running unpack (%lu, %lu, %lu, It: %u)\n", localId, lx, ly, lz, it); fflush(stdout);

//  if(subGrid.Down.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   for (int d = 0; d < gDepth; d++)
//    for (int y = 0; y < ls.y; y++)
//     for (int x = 0; x < ls.x; x++)
//      localU[fs.x*fs.y*(subGrid.lEnd.z+d) + fs.x*(subGrid.lStart.y+y) + (subGrid.lStart.x+x)] = subGrid.downRecvBuffer[it][bufferIdx++];
//   // free(subGrid.downRecvBuffer[it]); //Pop
//  }

//  if(subGrid.Up.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   for (int d = 0; d < gDepth; d++)
//    for (int y = 0; y < ls.y; y++)
//     for (int x = 0; x < ls.x; x++)
//      localU[fs.x*fs.y*d + fs.x*(subGrid.lStart.y+y) + (subGrid.lStart.x+x)] = subGrid.upRecvBuffer[it][bufferIdx++];
//   free(subGrid.upRecvBuffer[it]);
//  }

//  if(subGrid.East.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int y = 0; y < ls.y; y++)
//      localU[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lStart.y+y) + subGrid.lEnd.x + d] = subGrid.eastRecvBuffer[it][bufferIdx++];
//   free(subGrid.eastRecvBuffer[it]);
//  }

//  if(subGrid.West.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int y = 0; y < ls.y; y++)
//      localU[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lStart.y+y) + d] = subGrid.westRecvBuffer[it][bufferIdx++];
//   free(subGrid.westRecvBuffer[it]);
//  }

//  if(subGrid.North.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int x = 0; x < ls.x; x++)
//      localU[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*d + subGrid.lStart.x + x] = subGrid.northRecvBuffer[it][bufferIdx++];
//   free(subGrid.northRecvBuffer[it]);
//  }

//  if(subGrid.South.type  == REMOTE)
//   {
//    size_t bufferIdx = 0;
//    for (int d = 0; d < gDepth; d++)
//     for (int z = 0; z < ls.z; z++)
//      for (int x = 0; x < ls.x; x++)
//       localU[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lEnd.y+d) + subGrid.lStart.x + x] = subGrid.southRecvBuffer[it][bufferIdx++];
//    free(subGrid.southRecvBuffer[it]);
//   }

 // If we reached the iteration before the end, no more communication is needed
 if (it + 1 == nIters - 1) return;

 // Creating new task for the next iteration
 auto newTask = new Task("Unpack", lx, ly, lz, it+1, unpackFc);

 // Adding compute dependency for the next iteration
 newTask->addDependency(Task::encodeTaskName("Receive", lx, ly, lz, it+1));
 newTask->addDependency(Task::encodeTaskName("Compute", lx, ly, lz, it));

 // Creating task for the next iteration only if we haven't reached the end
 _taskr->addTask(newTask);
}

void Grid::pack(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
 auto localId = localSubGridMapping[lz][ly][lx];
 auto& subGrid = subgrids[localId];
 double *localUn = it % 2 == 0 ? Un : U;

//  if(subGrid.Down.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   subGrid.downSendBuffer[it] = (double*) malloc (sizeof(double) * bufferSizeZ); // Pre-allocate, for tmp usage only
//   for (int d = 0; d < gDepth; d++)
//    for (int y = 0; y < ls.y; y++)
//     for (int x = 0; x < ls.x; x++)
//      subGrid.downSendBuffer[it][bufferIdx++] = localUn[fs.x*fs.y*(subGrid.lEnd.z-gDepth+d) + fs.x*(subGrid.lStart.y+y) + (subGrid.lStart.x+x)];
//  }

//  if(subGrid.Up.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   subGrid.upSendBuffer[it] = (double*) malloc (sizeof(double) * bufferSizeZ);
//   for (int d = 0; d < gDepth; d++)
//    for (int y = 0; y < ls.y; y++)
//     for (int x = 0; x < ls.x; x++)
//      subGrid.upSendBuffer[it][bufferIdx++] = localUn[fs.x*fs.y*(subGrid.lStart.z+d) + fs.x*(subGrid.lStart.y+y) + (subGrid.lStart.x+x) ];
//  }

//  if(subGrid.East.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   subGrid.eastSendBuffer[it] = (double*) malloc (sizeof(double) * bufferSizeX);
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int y = 0; y < ls.y; y++)
//      subGrid.eastSendBuffer[it][bufferIdx++] = localUn[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lStart.y+y) + (subGrid.lEnd.x-gDepth+d)];
//  }

//  if(subGrid.West.type  == REMOTE)
//  {
//   subGrid.westSendBuffer[it] = (double*) malloc (sizeof(double) * bufferSizeX);
//   size_t bufferIdx = 0;
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int y = 0; y < ls.y; y++)
//      subGrid.westSendBuffer[it][bufferIdx++] = localUn[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lStart.y+y) + (subGrid.lStart.x+d)];
//  }

//  if(subGrid.North.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   subGrid.northSendBuffer[it] = (double*) malloc (sizeof(double) * bufferSizeY);
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int x = 0; x < ls.x; x++)
//      subGrid.northSendBuffer[it][bufferIdx++] = localUn[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lStart.y+d) + (subGrid.lStart.x+x)];
//  }

//  if(subGrid.South.type  == REMOTE)
//  {
//   size_t bufferIdx = 0;
//   subGrid.southSendBuffer[it] = (double*) malloc (sizeof(double) * bufferSizeY);
//   for (int d = 0; d < gDepth; d++)
//    for (int z = 0; z < ls.z; z++)
//     for (int x = 0; x < ls.x; x++)
//      subGrid.southSendBuffer[it][bufferIdx++] = localUn[fs.x*fs.y*(subGrid.lStart.z+z) + fs.x*(subGrid.lEnd.y-gDepth+d) + (subGrid.lStart.x+x)];
//  }

 // If we reached the iteration before the end, no more communication is needed
 if (it + 1 == nIters - 1) return;

 // Creating new task for the next iteration
 auto newTask = new Task("Pack", lx, ly, lz, it+1, packFc);

 // Adding compute dependency for the next iteration
 newTask->addDependency(Task::encodeTaskName("Compute", lx, ly, lz, it+1));

 // Creating task for the next iteration only if we haven't reached the end
 _taskr->addTask(newTask);
}

void Grid::send(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
{
 auto localId = localSubGridMapping[lz][ly][lx];
 auto& subGrid = subgrids[localId];

//  if(subGrid.Down.type  == REMOTE) MPI_Isend(subGrid.downSendBuffer[it],   bufferSizeZ, MPI_DOUBLE,  subGrid.Down.processId,  localId, MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.Up.type    == REMOTE) MPI_Isend(subGrid.upSendBuffer[it],     bufferSizeZ, MPI_DOUBLE,  subGrid.Up.processId,    localId, MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.East.type  == REMOTE) MPI_Isend(subGrid.eastSendBuffer[it],   bufferSizeX, MPI_DOUBLE,  subGrid.East.processId,  localId, MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.West.type  == REMOTE) MPI_Isend(subGrid.westSendBuffer[it],   bufferSizeX, MPI_DOUBLE,  subGrid.West.processId,  localId, MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.North.type == REMOTE) MPI_Isend(subGrid.northSendBuffer[it],  bufferSizeY, MPI_DOUBLE,  subGrid.North.processId, localId, MPI_COMM_WORLD, &requests[reqIdx++]);
//  if(subGrid.South.type == REMOTE) MPI_Isend(subGrid.southSendBuffer[it],  bufferSizeY, MPI_DOUBLE,  subGrid.South.processId, localId, MPI_COMM_WORLD, &requests[reqIdx++]);

 // If we reached the iteration before the end, no more communication is needed
 if (it + 1 == nIters - 1) return;

 // Creating new task for the next iteration
 auto newTask = new Task("Send", lx, ly, lz, it+1, sendFc);

 // Adding compute dependency for the next iteration
 newTask->addDependency(encodeTaskName("Pack", lx, ly, lz, it+1));

 // Creating task for the next iteration only if we haven't reached the end
 _taskr->addTask(newTask);
}

// void Grid::calculateResidual(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it)
// {
//  auto& t = subgrids[localSubGridMapping[lz][ly][lx]];
//  double *localU  = it % 2 == 0 ? Un :  U;

//  double err = 0;
//  for (int k=t.lStart.z; k<t.lEnd.z; k++)
//  for (int j=t.lStart.y; j<t.lEnd.y; j++)
//  for (int i=t.lStart.x; i<t.lEnd.x; i++)
//   { double r = localU[k*fs.y*fs.x + j*fs.x + i];  err += r * r; }

//  #pragma omp critical
//  _residual += err;
// }

// void Grid::print(const uint32_t it)
// {
//  double *localU  = it % 2 == 0 ? Un :  U;

//  for (size_t z = 0; z < fs.z; z++)
//  {
//   printf("Z Face %02lu\n", z);
//   printf("---------------------\n");

//   for (size_t y = 0; y < fs.y; y++)
//   {
//    for (size_t x = 0; x < fs.x; x++)
//    {
//      printf("%f ", localU[fs.x*fs.y*z + fs.x*y + x]);
//    }
//    printf("\n");
//   }
//  }
// }
