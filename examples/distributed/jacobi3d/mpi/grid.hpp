#include <cstdint>
const int BLOCKZ=96;
const int BLOCKY=64;

enum commTags {
 southTAG=1,
 northTAG=2,
 eastTAG=3,
 westTAG=4,
 downTAG=5,
 upTAG=6
};

enum commType {
   REMOTE=0,
    LOCAL,
  BOUNDARY
};

struct D3 {
 size_t x;
 size_t y;
 size_t z;
};

struct Neighbor {
 commType type;
 int processId;
 D3 lPos;
};

class Grid
{
 public:

 // Configuration
 int processId; // Id for the current global process
 size_t nIters; // Number of iterations
 size_t N; // Grid ls per side (N)
 int gDepth; // Ghost cell depth
 double invCoeff;

 // Grid containers
 double* U;
 double* Un;

 // Grid Topology Management
 D3 ps; // Grid Size per Process
 D3 pt; // Global process topology
 D3 fs; // Effective Grid Size (including ghost cells)
 D3 pPos; // process location information
 D3 start; // Grid pos start
 D3 end; // Grid pos end

 // Neighbor information
 Neighbor East;
 Neighbor West;
 Neighbor North;
 Neighbor South;
 Neighbor Up;
 Neighbor Down;

 public:

 double _localResidual;

void print(const uint32_t it);
 Grid(const int processId, const size_t N, const size_t nIters, const size_t gDepth, const D3& pt);
 bool initialize();
 void finalize();
 void solve();
 void reset();
 double calculateResidual(const uint32_t it);
};

