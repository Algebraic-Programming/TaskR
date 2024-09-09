#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include "grid.hpp"

Grid::Grid(const int processId, const size_t N, const size_t nIters, const size_t gDepth, const D3& pt)
{
 // Copying values
 this->processId = processId;
 this->N = N;
 this->nIters = nIters;
 this->gDepth = gDepth;
 this->pt = pt;
}

bool Grid::initialize()
{
 // Inverse stencil coefficient
 invCoeff = 1.0 / (1.0 + 6.0 * (double)gDepth);

 // Checking topology correctness
 if(N % pt.x > 0) { fprintf(stderr, "Error: N (%lu) should be divisible by px (%lu)\n", N, pt.x); return false; }
 if(N % pt.y > 0) { fprintf(stderr, "Error: N (%lu) should be divisible by py (%lu)\n", N, pt.y); return false; }
 if(N % pt.z > 0) { fprintf(stderr, "Error: N (%lu) should be divisible by pz (%lu)\n", N, pt.z); return false; }

 // Calculating grid ps per process
 ps.x = N / pt.x;
 ps.y = N / pt.y;
 ps.z = N / pt.z;

 // Calculating grid ps per process plus ghost cells
 fs.x = ps.x + 2 * gDepth;
 fs.y = ps.y + 2 * gDepth;
 fs.z = ps.z + 2 * gDepth;

 U  = (double *)malloc(sizeof(double)*fs.x*fs.y*fs.z);
 Un = (double *)malloc(sizeof(double)*fs.x*fs.y*fs.z);

 size_t processCount = pt.x * pt.y * pt.z;
 int* globalRankX = (int*) calloc(sizeof(int),processCount);
 int* globalRankY = (int*) calloc(sizeof(int),processCount);
 int* globalRankZ = (int*) calloc(sizeof(int),processCount);
 int*** globalProcessMapping = (int***) calloc(sizeof(int**),pt.z);
 for (int i = 0; i < pt.z; i++) globalProcessMapping[i] = (int**) calloc (sizeof(int*),pt.y);
 for (int i = 0; i < pt.z; i++) for (int j = 0; j < pt.y; j++) globalProcessMapping[i][j] = (int*) calloc (sizeof(int),pt.x);

 int currentRank = 0;
	for (int z = 0; z < pt.z; z++)
	for (int y = 0; y < pt.y; y++)
	for (int x = 0; x < pt.x; x++)
	{ globalRankZ[currentRank] = z; globalRankX[currentRank] = x; globalRankY[currentRank] = y; globalProcessMapping[z][y][x] = currentRank; currentRank++; }

 //  if (processId == 0) for (int i = 0; i < processCount; i++) printf("Rank %d - Z: %d, Y: %d, X: %d\n", i, globalRankZ[i], globalRankY[i], globalRankX[i]);

 start.x = gDepth;
 start.y = gDepth;
 start.z = gDepth;
 end.x = start.x + ps.x;
 end.y = start.y + ps.y;
 end.z = start.z + ps.z;

 pPos.z = globalRankZ[processId];
 pPos.y = globalRankY[processId];
 pPos.x = globalRankX[processId];

 West.type  = REMOTE; West.processId  = processId;
 East.type  = REMOTE; East.processId  = processId;
 North.type = REMOTE; North.processId = processId;
 South.type = REMOTE; South.processId = processId;
 Up.type    = REMOTE; Up.processId    = processId;
 Down.type  = REMOTE; Down.processId  = processId;

 if (pPos.x == 0)    West.type  = BOUNDARY;
 if (pPos.y == 0)    North.type = BOUNDARY;
 if (pPos.z == 0)    Up.type    = BOUNDARY;
 if (pPos.x == pt.x-1) East.type  = BOUNDARY;
 if (pPos.y == pt.y-1) South.type = BOUNDARY;
 if (pPos.z == pt.z-1) Down.type  = BOUNDARY;

 if(West.type  == REMOTE) West.processId  = globalProcessMapping[pPos.z][pPos.y][pPos.x-1];
 if(East.type  == REMOTE) East.processId  = globalProcessMapping[pPos.z][pPos.y][pPos.x+1];
 if(North.type == REMOTE) North.processId = globalProcessMapping[pPos.z][pPos.y-1][pPos.x];
 if(South.type == REMOTE) South.processId = globalProcessMapping[pPos.z][pPos.y+1][pPos.x];
 if(Up.type    == REMOTE) Up.processId    = globalProcessMapping[pPos.z-1][pPos.y][pPos.x];
 if(Down.type  == REMOTE) Down.processId  = globalProcessMapping[pPos.z+1][pPos.y][pPos.x];

 free(globalRankX);
 free(globalRankY);
 free(globalRankZ);

 for (int i = 0; i < pt.z; i++) for (int j = 0; j < pt.y; j++) free(globalProcessMapping[i][j]);
 for (int i = 0; i < pt.z; i++) free(globalProcessMapping[i]);
	free(globalProcessMapping);

 return true;
}

void Grid::finalize()
{
 free(U);
 free(Un);
}

void Grid::reset()
{
 for (int k = start.z-gDepth; k < end.z+gDepth; k++)
 for (int j = start.y-gDepth; j < end.y+gDepth; j++)
 for (int i = start.x-gDepth; i < end.x+gDepth; i++)
   U[k*fs.y*fs.x + j*fs.x + i] = 1;
//
 if (West.type  == BOUNDARY) for (int i = start.y-gDepth; i < end.y+gDepth; i++) for (int j = start.z-gDepth; j < end.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + i*fs.x + d] = 0;
 if (North.type == BOUNDARY) for (int i = start.x-gDepth; i < end.x+gDepth; i++) for (int j = start.z-gDepth; j < end.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + d*fs.x + i] = 0;
 if (Up.type    == BOUNDARY) for (int i = start.x-gDepth; i < end.x+gDepth; i++) for (int j = start.y-gDepth; j < end.y+gDepth; j++) for (int d = 0; d < gDepth; d++) U[d*fs.x*fs.y + j*fs.x + i] = 0;
 if (East.type  == BOUNDARY) for (int i = start.y-gDepth; i < end.y+gDepth; i++) for (int j = start.z-gDepth; j < end.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + i*fs.x + (ps.x+gDepth+d)] = 0;
 if (South.type == BOUNDARY) for (int i = start.x-gDepth; i < end.x+gDepth; i++) for (int j = start.z-gDepth; j < end.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + (ps.y+gDepth+d)*fs.x + i] = 0;
 if (Down.type  == BOUNDARY) for (int i = start.x-gDepth; i < end.x+gDepth; i++) for (int j = start.y-gDepth; j < end.y+gDepth; j++) for (int d = 0; d < gDepth; d++) U[(ps.z+gDepth+d)*fs.x*fs.y + j*fs.x + i] = 0;
}

void Grid::solve()
{
 int bufferSizeX = ps.y*ps.z*gDepth;
 int bufferSizeY = ps.x*ps.z*gDepth;
 int bufferSizeZ = ps.x*ps.y*gDepth;

 double* tmpU = U;
 double* tmpUn = Un;

 double* upSendBuffer    = (double*) calloc (sizeof(double), bufferSizeZ);
 double* downSendBuffer  = (double*) calloc (sizeof(double), bufferSizeZ);
 double* eastSendBuffer  = (double*) calloc (sizeof(double), bufferSizeX);
 double* westSendBuffer  = (double*) calloc (sizeof(double), bufferSizeX);
 double* northSendBuffer = (double*) calloc (sizeof(double), bufferSizeY);
 double* southSendBuffer = (double*) calloc (sizeof(double), bufferSizeY);
 double* upRecvBuffer    = (double*) calloc (sizeof(double), bufferSizeZ);
 double* downRecvBuffer  = (double*) calloc (sizeof(double), bufferSizeZ);
 double* eastRecvBuffer  = (double*) calloc (sizeof(double), bufferSizeX);
 double* westRecvBuffer  = (double*) calloc (sizeof(double), bufferSizeX);
 double* northRecvBuffer = (double*) calloc (sizeof(double), bufferSizeY);
 double* southRecvBuffer = (double*) calloc (sizeof(double), bufferSizeY);

 // Inverse stencil coefficient
 MPI_Request request[12];

 for (int iter=0; iter < nIters; iter++)
 {

  // Computing
  for (int k0 = start.z; k0 < end.z; k0+=BLOCKZ) {
   int k1= k0+BLOCKZ<end.z?k0+BLOCKZ:end.z;
   for (int j0 = start.y; j0 < end.y; j0+=BLOCKY) {
    int j1= j0+BLOCKY<end.y?j0+BLOCKY:end.y;
    for (int k = k0; k < k1; k++) {
      for (int j = j0; j < j1; j++){
       #pragma vector aligned
       #pragma ivdep
       for (int i = start.x; i < end.x; i++)
       {
        double sum = tmpU[fs.x*fs.y*k     + fs.x*j         + i]; // Central
        for (int d = 1; d <= gDepth; d++)
        {
         sum += tmpU[fs.x*fs.y*(k-d) + fs.x*j         + i]; // Up
         sum += tmpU[fs.x*fs.y*(k+d) + fs.x*j         + i]; // Down
         sum += tmpU[fs.x*fs.y*k     + fs.x*j     - d + i]; // East
         sum += tmpU[fs.x*fs.y*k     + fs.x*j     + d + i]; // West
         sum += tmpU[fs.x*fs.y*k     + fs.x*(j+d)     + i]; // North
         sum += tmpU[fs.x*fs.y*k     + fs.x*(j-d)     + i]; // South
        }
        tmpUn[fs.x*fs.y*k     + fs.x*j + i] = sum * invCoeff; // Update
       }
      }
    }
   }
  }
  std::swap(tmpU, tmpUn);

  int requestCount = 0;

  // Receiving
  if(Down.type  == REMOTE) MPI_Irecv(downRecvBuffer,  bufferSizeZ,  MPI_DOUBLE, Down.processId,  upTAG,    MPI_COMM_WORLD, &request[requestCount++]);
  if(Up.type    == REMOTE) MPI_Irecv(upRecvBuffer,    bufferSizeZ,  MPI_DOUBLE, Up.processId,    downTAG,  MPI_COMM_WORLD, &request[requestCount++]);
  if(East.type  == REMOTE) MPI_Irecv(eastRecvBuffer,  bufferSizeX,  MPI_DOUBLE, East.processId,  westTAG,  MPI_COMM_WORLD, &request[requestCount++]);
  if(West.type  == REMOTE) MPI_Irecv(westRecvBuffer,  bufferSizeX,  MPI_DOUBLE, West.processId,  eastTAG,  MPI_COMM_WORLD, &request[requestCount++]);
  if(North.type == REMOTE) MPI_Irecv(northRecvBuffer, bufferSizeY,  MPI_DOUBLE, North.processId, southTAG, MPI_COMM_WORLD, &request[requestCount++]);
  if(South.type == REMOTE) MPI_Irecv(southRecvBuffer, bufferSizeY,  MPI_DOUBLE, South.processId, northTAG, MPI_COMM_WORLD, &request[requestCount++]);

  // Packing
  if(Down.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int y = 0; y < ps.y; y++)
     for (int x = 0; x < ps.x; x++)
      downSendBuffer[bufferIdx++] = tmpU[fs.x*fs.y*(end.z-gDepth+d) + fs.x*(start.y+y) + (start.x+x)];
  }

  if(Up.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int y = 0; y < ps.y; y++)
     for (int x = 0; x < ps.x; x++)
      upSendBuffer[bufferIdx++] = tmpU[fs.x*fs.y*(start.z+d) + fs.x*(start.y+y) + (start.x+x) ];
  }

  if(East.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int y = 0; y < ps.y; y++)
      eastSendBuffer[bufferIdx++] = tmpU[fs.x*fs.y*(start.z+z) + fs.x*(start.y+y) + (end.x-gDepth+d)];
  }

  if(West.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int y = 0; y < ps.y; y++)
      westSendBuffer[bufferIdx++] = tmpU[fs.x*fs.y*(start.z+z) + fs.x*(start.y+y) + (start.x+d)];
  }

  if(North.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int x = 0; x < ps.x; x++)
      northSendBuffer[bufferIdx++] = tmpU[fs.x*fs.y*(start.z+z) + fs.x*(start.y+d) + (start.x+x)];
  }

  if(South.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int x = 0; x < ps.x; x++)
      southSendBuffer[bufferIdx++] = tmpU[fs.x*fs.y*(start.z+z) + fs.x*(end.y-gDepth+d) + (start.x+x)];
  }

  // Sending
  if(Down.type  == REMOTE) MPI_Isend(downSendBuffer,  bufferSizeZ, MPI_DOUBLE, Down.processId,  downTAG,  MPI_COMM_WORLD, &request[requestCount++]);
  if(Up.type    == REMOTE) MPI_Isend(upSendBuffer,    bufferSizeZ, MPI_DOUBLE, Up.processId,    upTAG,    MPI_COMM_WORLD, &request[requestCount++]);
  if(East.type  == REMOTE) MPI_Isend(eastSendBuffer,  bufferSizeX, MPI_DOUBLE, East.processId,  eastTAG,  MPI_COMM_WORLD, &request[requestCount++]);
  if(West.type  == REMOTE) MPI_Isend(westSendBuffer,  bufferSizeX, MPI_DOUBLE, West.processId,  westTAG,  MPI_COMM_WORLD, &request[requestCount++]);
  if(North.type == REMOTE) MPI_Isend(northSendBuffer, bufferSizeY, MPI_DOUBLE, North.processId, northTAG, MPI_COMM_WORLD, &request[requestCount++]);
  if(South.type == REMOTE) MPI_Isend(southSendBuffer, bufferSizeY, MPI_DOUBLE, South.processId, southTAG, MPI_COMM_WORLD, &request[requestCount++]);

  // Waiting
  MPI_Waitall(requestCount, request, MPI_STATUS_IGNORE);

  // Unpacking
  if(Down.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int y = 0; y < ps.y; y++)
     for (int x = 0; x < ps.x; x++)
      tmpU[fs.x*fs.y*(end.z+d) + fs.x*(start.y+y) + (start.x+x)] = downRecvBuffer[bufferIdx++];
  }

  if(Up.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int y = 0; y < ps.y; y++)
     for (int x = 0; x < ps.x; x++)
      tmpU[fs.x*fs.y*d + fs.x*(start.y+y) + (start.x+x)] = upRecvBuffer[bufferIdx++];
  }

  if(East.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int y = 0; y < ps.y; y++)
      tmpU[fs.x*fs.y*(start.z+z) + fs.x*(start.y+y) + end.x + d] = eastRecvBuffer[bufferIdx++];
  }

  if(West.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int y = 0; y < ps.y; y++)
      tmpU[fs.x*fs.y*(start.z+z) + fs.x*(start.y+y) + d] = westRecvBuffer[bufferIdx++];
  }

  if(North.type  == REMOTE)
  {
   size_t bufferIdx = 0;
   for (int d = 0; d < gDepth; d++)
    for (int z = 0; z < ps.z; z++)
     for (int x = 0; x < ps.x; x++)
      tmpU[fs.x*fs.y*(start.z+z) + fs.x*d + start.x + x] = northRecvBuffer[bufferIdx++];
  }

  if(South.type  == REMOTE)
   {
    size_t bufferIdx = 0;
    for (int d = 0; d < gDepth; d++)
     for (int z = 0; z < ps.z; z++)
      for (int x = 0; x < ps.x; x++)
       tmpU[fs.x*fs.y*(start.z+z) + fs.x*(end.y+d) + start.x + x] = southRecvBuffer[bufferIdx++];
   }
 }

 // Freeing buffers
 free(upSendBuffer);
 free(downSendBuffer);
 free(eastSendBuffer);
 free(westSendBuffer);
 free(northSendBuffer);
 free(southSendBuffer);
 free(upRecvBuffer);
 free(downRecvBuffer);
 free(eastRecvBuffer);
 free(westRecvBuffer);
 free(northRecvBuffer);
 free(southRecvBuffer);
}

double Grid::calculateResidual()
{
 double res = 0;
 double err = 0;
 for (int k=start.z; k<end.z; k++)
 for (int j=start.y; j<end.y; j++)
 for (int i=start.x; i<end.x; i++)
  { double r = U[k*fs.y*fs.x + j*fs.x + i];  err += r * r; }
 MPI_Reduce (&err, &res, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
 res = sqrt(res/((double)(N-1)*(double)(N-1)*(double)(N-1)));

 return res;
}
