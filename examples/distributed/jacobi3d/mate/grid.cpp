#include <mpi.h>
#include <stdio.h>
#include <math.h>
#include "grid.hpp"

Grid::Grid(const int processId, const size_t N, const size_t nIters, const size_t gDepth,const D3& pt,const D3& lt)
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

	int curLocalRank = 0;
	int*** localRankMapping = (int***) calloc (sizeof(int**) , lt.z);
	for (int i = 0; i < lt.z; i++) localRankMapping[i] = (int**) calloc (sizeof(int*) , lt.y);
	for (int i = 0; i < lt.z; i++) for (int j = 0; j < lt.y; j++) localRankMapping[i][j] = (int*) calloc (sizeof(int) , lt.x);
	for (int i = 0; i < lt.z; i++) for (int j = 0; j < lt.y; j++) for (int k = 0; k < lt.x; k++) localRankMapping[i][j][k] = curLocalRank++;

	// Getting process-wise mapping
 pPos.z = globalRankZ[processId];
 pPos.y = globalRankY[processId];
 pPos.x = globalRankX[processId];

 // Grid size for local tasks
 ls.x = ps.x / lt.x;
 ls.y = ps.y / lt.y;
 ls.z = ps.z / lt.z;

 // Mapping for local tasks
 size_t localRankCount = lt.x * lt.y * lt.z;
	subgrids.resize(localRankCount);
	for (size_t localId = 0; localId < localRankCount; localId++)
	{
	 auto& t = subgrids[localId];

   // Processing local task mapping
  for(int i = 0; i < lt.z; i++)
  for (int j = 0; j < lt.y; j++)
  for (int k = 0; k < lt.x; k++)
   if (localRankMapping[i][j][k] == localId)
    { t.lPos.z = i;  t.lPos.y = j; t.lPos.x = k;}

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

  t.West.localId  = localRankMapping[t.lPos.z][t.lPos.y][t.West.lPos.x];
  t.East.localId  = localRankMapping[t.lPos.z][t.lPos.y][t.East.lPos.x];
  t.North.localId = localRankMapping[t.lPos.z][t.North.lPos.y][t.lPos.x];
  t.South.localId = localRankMapping[t.lPos.z][t.South.lPos.y][t.lPos.x];
  t.Up.localId    = localRankMapping[t.Up.lPos.z][t.lPos.y][t.lPos.x];
  t.Down.localId  = localRankMapping[t.Down.lPos.z][t.lPos.y][t.lPos.x];

  if (t.West.type  == REMOTE) t.West.processId  = globalProcessMapping[pPos.z][pPos.y][pPos.x-1];
  if (t.East.type  == REMOTE) t.East.processId  = globalProcessMapping[pPos.z][pPos.y][pPos.x+1];
  if (t.North.type == REMOTE) t.North.processId = globalProcessMapping[pPos.z][pPos.y-1][pPos.x];
  if (t.South.type == REMOTE) t.South.processId = globalProcessMapping[pPos.z][pPos.y+1][pPos.x];
  if (t.Up.type    == REMOTE) t.Up.processId    = globalProcessMapping[pPos.z-1][pPos.y][pPos.x];
  if (t.Down.type  == REMOTE) t.Down.processId  = globalProcessMapping[pPos.z+1][pPos.y][pPos.x];
	}

 free(globalRankX);
 free(globalRankY);
 free(globalRankZ);

 for (int i = 0; i < pt.z; i++) for (int j = 0; j < pt.y; j++) free(globalProcessMapping[i][j]);
 for (int i = 0; i < pt.z; i++) free(globalProcessMapping[i]);
	free(globalProcessMapping);

 for (int i = 0; i < lt.z; i++) for (int j = 0; j < lt.y; j++) free(localRankMapping[i][j]);
 for (int i = 0; i < lt.z; i++) free(localRankMapping[i]);
 free(localRankMapping);

	// Creating message types
 MPI_Type_vector(ls.y, 1, fs.x, MPI_DOUBLE, &faceX_subtype);
 MPI_Type_commit(&faceX_subtype);

 int* counts = (int*) calloc (sizeof(int) , ls.z);
 MPI_Aint* displs = (MPI_Aint*) calloc (sizeof(MPI_Aint) , ls.z);
 MPI_Datatype* types = (MPI_Datatype*) calloc (sizeof(MPI_Datatype) , ls.z);
 for (int i = 0; i < ls.z; i++) { counts[i] = 1;  displs[i] = fs.y*fs.x*sizeof(double)*i; types[i] = faceX_subtype; }

 MPI_Type_struct(ls.z, counts, displs, types, &faceX_type);
 MPI_Type_vector(ls.z, ls.x, fs.x*fs.y, MPI_DOUBLE, &faceY_type);
 MPI_Type_vector(ls.y, ls.x, fs.x, MPI_DOUBLE, &faceZ_type);

 MPI_Type_commit(&faceX_type);
 MPI_Type_commit(&faceY_type);
 MPI_Type_commit(&faceZ_type);

 return true;
}

void Grid::finalize()
{
 free(U);
 free(Un);

 MPI_Type_free(&faceX_subtype);
 MPI_Type_free(&faceX_type);
 MPI_Type_free(&faceY_type);
 MPI_Type_free(&faceZ_type);
}

void Grid::reset()
{
 int localId = 0;
 Mate_local_rank_id(&localId);
 auto& t = subgrids[localId];

 for (int k = t.lStart.z-gDepth; k < t.lEnd.z+gDepth; k++)
 for (int j = t.lStart.y-gDepth; j < t.lEnd.y+gDepth; j++)
 for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++)
   U[k*fs.y*fs.x + j*fs.x + i] = 1;

 if (t.West.type  == BOUNDARY) for (int i = t.lStart.y-gDepth; i < t.lEnd.y+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + i*fs.x + d] = 0;
 if (t.North.type == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + d*fs.x + i] = 0;
 if (t.Up.type    == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.y-gDepth; j < t.lEnd.y+gDepth; j++) for (int d = 0; d < gDepth; d++) U[d*fs.x*fs.y + j*fs.x + i] = 0;
 if (t.East.type  == BOUNDARY) for (int i = t.lStart.y-gDepth; i < t.lEnd.y+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + i*fs.x + (ps.x+gDepth+d)] = 0;
 if (t.South.type == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.z-gDepth; j < t.lEnd.z+gDepth; j++) for (int d = 0; d < gDepth; d++) U[j*fs.x*fs.y + (ps.y+gDepth+d)*fs.x + i] = 0;
 if (t.Down.type  == BOUNDARY) for (int i = t.lStart.x-gDepth; i < t.lEnd.x+gDepth; i++) for (int j = t.lStart.y-gDepth; j < t.lEnd.y+gDepth; j++) for (int d = 0; d < gDepth; d++) U[(ps.z+gDepth+d)*fs.x*fs.y + j*fs.x + i] = 0;
}

void Grid::solve()
{
 int localId = 0;
 Mate_local_rank_id(&localId);
 auto& t = subgrids[localId];

 // Local pointer copies
 double *localU = U;
 double *localUn = Un;

 // Inverse stencil coefficient
 double invCoeff = 1.0 / (7.0 * (double)gDepth);

 // MPI Request buffers
 MPI_Request recvRequests[6];
 MPI_Request sendRequests[6];

 int bufferSizeX = ls.y*ls.z*gDepth;
 int bufferSizeY = ls.x*ls.z*gDepth;
 int bufferSizeZ = ls.x*ls.y*gDepth;

 // Allocating buffers
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

 // printf("Local Task: %u, Start: (%lu, %lu, %lu), End: (%lu, %lu, %lu)\n", localId, t.lStart.x, t.lStart.y, t.lStart.z, t.lEnd.x, t.lEnd.y, t.lEnd.z);

 if (t.West.type  == LOCAL) Mate_AddLocalNeighbor(t.West.localId);
 if (t.East.type  == LOCAL) Mate_AddLocalNeighbor(t.East.localId);
 if (t.North.type == LOCAL) Mate_AddLocalNeighbor(t.North.localId);
 if (t.South.type == LOCAL) Mate_AddLocalNeighbor(t.South.localId);
 if (t.Up.type    == LOCAL) Mate_AddLocalNeighbor(t.Up.localId);
 if (t.Down.type  == LOCAL) Mate_AddLocalNeighbor(t.Down.localId);

 #pragma mate graph
 for (int iter=0; iter<nIters; iter++)
 {
  #pragma mate region(compute) depends(pack*, unpack*, compute*@)
  {
   for (int k0 = t.lStart.z; k0 < t.lEnd.z; k0+=BLOCKZ) {
    int k1= k0+BLOCKZ<t.lEnd.z?k0+BLOCKZ:t.lEnd.z;
    for (int j0 = t.lStart.y; j0 < t.lEnd.y; j0+=BLOCKY) {
     int j1= j0+BLOCKY<t.lEnd.y?j0+BLOCKY:t.lEnd.y;
     for (int k = k0; k < k1; k++) {
       for (int j = j0; j < j1; j++){
        #pragma vector aligned
        #pragma ivdep
        for (int i = t.lStart.x; i < t.lEnd.x; i++)
        {
         double sum = localU[fs.x*fs.y*k     + fs.x*j + i]; // Central
         for (int d = 1; d <= gDepth; d++)
         {
          double partial_sum = 0;
          sum += localU[fs.x*fs.y*(k-d) + fs.x*j         + i]; // Up
          sum += localU[fs.x*fs.y*(k+d) + fs.x*j         + i]; // Down
          sum += localU[fs.x*fs.y*k     + fs.x*j     - d + i]; // East
          sum += localU[fs.x*fs.y*k     + fs.x*j     + d + i]; // West
          sum += localU[fs.x*fs.y*k     + fs.x*(j+d)     + i]; // North
          sum += localU[fs.x*fs.y*k     + fs.x*(j-d)     + i]; // South
         }
         localUn[fs.x*fs.y*k     + fs.x*j + i] = sum * invCoeff; // Update
        }
       }
     }
    }
   }
   std::swap(localU, localUn);
  }

  #pragma mate region (request)  depends (unpack*)
  {
   int reqIdx = 0;
   if(t.Down.type  == REMOTE) Mate_Recv(downRecvBuffer,  bufferSizeZ, MPI_DOUBLE, t.Down.processId,  t.Down.localId,  upTAG,    MPI_COMM_WORLD, &recvRequests[reqIdx++]);
   if(t.Up.type    == REMOTE) Mate_Recv(upRecvBuffer,    bufferSizeZ, MPI_DOUBLE, t.Up.processId,    t.Up.localId,    downTAG,  MPI_COMM_WORLD, &recvRequests[reqIdx++]);
   if(t.East.type  == REMOTE) Mate_Recv(eastRecvBuffer,  bufferSizeX, MPI_DOUBLE, t.East.processId,  t.East.localId,  westTAG,  MPI_COMM_WORLD, &recvRequests[reqIdx++]);
   if(t.West.type  == REMOTE) Mate_Recv(westRecvBuffer,  bufferSizeX, MPI_DOUBLE, t.West.processId,  t.West.localId,  eastTAG,  MPI_COMM_WORLD, &recvRequests[reqIdx++]);
   if(t.North.type == REMOTE) Mate_Recv(northRecvBuffer, bufferSizeY, MPI_DOUBLE, t.North.processId, t.North.localId, southTAG, MPI_COMM_WORLD, &recvRequests[reqIdx++]);
   if(t.South.type == REMOTE) Mate_Recv(southRecvBuffer, bufferSizeY, MPI_DOUBLE, t.South.processId, t.South.localId, northTAG, MPI_COMM_WORLD, &recvRequests[reqIdx++]);
  }

  #pragma mate region (pack)  depends (compute, send*)
  {
   if(t.Down.type  == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Pack(&localU[fs.x*fs.y*(t.lEnd.z-gDepth+d) + fs.x*t.lStart.y            + t.lStart.x              ], 1, faceZ_type,  downSendBuffer,  sizeof(double)*bufferSizeZ, &bufferOffset, MPI_COMM_WORLD); }
   if(t.Up.type    == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Pack(&localU[fs.x*fs.y*(t.lStart.z+d)      + fs.x*t.lStart.y            + t.lStart.x              ], 1, faceZ_type,  upSendBuffer,    sizeof(double)*bufferSizeZ, &bufferOffset, MPI_COMM_WORLD); }
   if(t.East.type  == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Pack(&localU[fs.x*fs.y*t.lStart.z          + fs.x*t.lStart.y            + (t.lEnd.x-gDepth+d)     ], 1, faceX_type,  eastSendBuffer,  sizeof(double)*bufferSizeX, &bufferOffset, MPI_COMM_WORLD); }
   if(t.West.type  == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Pack(&localU[fs.x*fs.y*t.lStart.z          + fs.x*t.lStart.y            + (t.lStart.x+d)          ], 1, faceX_type,  westSendBuffer,  sizeof(double)*bufferSizeX, &bufferOffset, MPI_COMM_WORLD); }
   if(t.North.type == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Pack(&localU[fs.x*fs.y*t.lStart.z          + fs.x*(t.lStart.y+d)        + t.lStart.x              ], 1, faceY_type,  northSendBuffer, sizeof(double)*bufferSizeY, &bufferOffset, MPI_COMM_WORLD); }
   if(t.South.type == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Pack(&localU[fs.x*fs.y*t.lStart.z          + fs.x*(t.lEnd.y-gDepth+d)   + t.lStart.x              ], 1, faceY_type,  southSendBuffer, sizeof(double)*bufferSizeY, &bufferOffset, MPI_COMM_WORLD); }
  }

  #pragma mate region (send) depends (pack)
  {
   int reqIdx = 0;
   if(t.Down.type  == REMOTE) Mate_Send(downSendBuffer,   bufferSizeZ, MPI_DOUBLE,  t.Down.processId,  t.Down.localId,  downTAG,  MPI_COMM_WORLD, &sendRequests[reqIdx++]);
   if(t.Up.type    == REMOTE) Mate_Send(upSendBuffer,     bufferSizeZ, MPI_DOUBLE,  t.Up.processId,    t.Up.localId,    upTAG,    MPI_COMM_WORLD, &sendRequests[reqIdx++]);
   if(t.East.type  == REMOTE) Mate_Send(eastSendBuffer,   bufferSizeX, MPI_DOUBLE,  t.East.processId,  t.East.localId,  eastTAG,  MPI_COMM_WORLD, &sendRequests[reqIdx++]);
   if(t.West.type  == REMOTE) Mate_Send(westSendBuffer,   bufferSizeX, MPI_DOUBLE,  t.West.processId,  t.West.localId,  westTAG,  MPI_COMM_WORLD, &sendRequests[reqIdx++]);
   if(t.North.type == REMOTE) Mate_Send(northSendBuffer,  bufferSizeY, MPI_DOUBLE,  t.North.processId, t.North.localId, northTAG, MPI_COMM_WORLD, &sendRequests[reqIdx++]);
   if(t.South.type == REMOTE) Mate_Send(southSendBuffer,  bufferSizeY, MPI_DOUBLE,  t.South.processId, t.South.localId, southTAG, MPI_COMM_WORLD, &sendRequests[reqIdx++]);
  }

  #pragma mate region (unpack) depends (compute, request)
  {
   if(t.Down.type  == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Unpack(downRecvBuffer,  sizeof(double)*bufferSizeZ, &bufferOffset, &localU[fs.x*fs.y*(t.lEnd.z+d) + fs.x*t.lStart.y    + t.lStart.x   ], 1, faceZ_type, MPI_COMM_WORLD); }
   if(t.Up.type    == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Unpack(upRecvBuffer,    sizeof(double)*bufferSizeZ, &bufferOffset, &localU[fs.x*fs.y*d            + fs.x*t.lStart.y    + t.lStart.x   ], 1, faceZ_type, MPI_COMM_WORLD); }
   if(t.East.type  == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Unpack(eastRecvBuffer,  sizeof(double)*bufferSizeX, &bufferOffset, &localU[fs.x*fs.y*t.lStart.z   + fs.x*t.lStart.y    + t.lEnd.x + d ], 1, faceX_type, MPI_COMM_WORLD); }
   if(t.West.type  == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Unpack(westRecvBuffer,  sizeof(double)*bufferSizeX, &bufferOffset, &localU[fs.x*fs.y*t.lStart.z   + fs.x*t.lStart.y    + d            ], 1, faceX_type, MPI_COMM_WORLD); }
   if(t.North.type == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Unpack(northRecvBuffer, sizeof(double)*bufferSizeY, &bufferOffset, &localU[fs.x*fs.y*t.lStart.z   + fs.x*d             + t.lStart.x   ], 1, faceY_type, MPI_COMM_WORLD); }
   if(t.South.type == REMOTE) { int bufferOffset = 0; for (int d = 0; d < gDepth; d++) Mate_Unpack(southRecvBuffer, sizeof(double)*bufferSizeY, &bufferOffset, &localU[fs.x*fs.y*t.lStart.z   + fs.x*(t.lEnd.y+d)  + t.lStart.x   ], 1, faceY_type, MPI_COMM_WORLD); }
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
 int localId = 0;
 Mate_local_rank_id(&localId);
 auto& t = subgrids[localId];

 double res = 0;
 double err = 0;
 for (int k=t.lStart.z; k<t.lEnd.z; k++)
 for (int j=t.lStart.y; j<t.lEnd.y; j++)
 for (int i=t.lStart.x; i<t.lEnd.x; i++)
  { double r = U[k*fs.y*fs.x + j*fs.x + i];  err += r * r; }
 MPI_Reduce (&err, &res, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
 res = sqrt(res/((double)(N-1)*(double)(N-1)*(double)(N-1)));

 return res;
}
