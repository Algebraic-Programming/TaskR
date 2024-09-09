#include <vector>

const int BLOCKZ=96;
const int BLOCKY=64;

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
	int localId;
	D3 lPos;
};

struct SubGrid {
 Neighbor East, West, North, South, Up, Down;
 D3 lPos;
 D3 lStart;
 D3 lEnd;

 std::vector<double*> upSendBuffer;
 std::vector<double*> downSendBuffer;
 std::vector<double*> eastSendBuffer;
 std::vector<double*> westSendBuffer;
 std::vector<double*> northSendBuffer;
 std::vector<double*> southSendBuffer;

 std::vector<double*> upRecvBuffer;
 std::vector<double*> downRecvBuffer;
 std::vector<double*> eastRecvBuffer;
 std::vector<double*> westRecvBuffer;
 std::vector<double*> northRecvBuffer;
 std::vector<double*> southRecvBuffer;
};

class Grid
{
 public:

 // Configuration
 int processId; // Id for the current global process
 size_t nIters; // Number of iterations
 size_t N; // Grid ls per side (N)
 int gDepth; // Ghost cell depth
 double invCoeff; // Pre-calculated inverse coefficient

 // Grid containers
 double* U;
 double* Un;

 // Grid Topology Management
 D3 ps; // Grid Size per Process
 D3 pt; // Global process topology
 D3 lt; // Local task topology
 D3 fs; // Effective Grid Size (including ghost cells)
 D3 ls; // Local Grid Size
 D3 pPos; // process location information
 int*** globalProcessMapping; // MPI process mapping
 size_t localRankCount;

 // Local subgrid information
 int*** localSubGridMapping;
 std::vector<SubGrid> subgrids;

 // Number of elements per face
 size_t faceSizeX;
 size_t faceSizeY;
 size_t faceSizeZ;

 // Size of MPI buffers
 size_t bufferSizeX;
 size_t bufferSizeY;
 size_t bufferSizeZ;

 // Storage for L2 residual calculation
 double _residual;

 public:

 Grid(const int processId, const size_t N, const size_t nIters, const size_t gDepth, const D3& pt, const D3& lt);
 bool initialize();
 void finalize();
 void print(const uint32_t it);
 void compute(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
 void receive(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
 void send(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
 void pack(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
 void unpack(const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint32_t it);
 void reset(const uint64_t lx, const uint64_t ly, const uint64_t lz);
 void calculateResidual(const uint64_t lx, const uint64_t ly, const uint64_t lz,  const uint32_t it);
 int getMPITag(const int srcId, const int dstId);

 static inline std::string encodeTaskName(const std::string& taskName, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint64_t iter)
 {
  char buffer[512];
  sprintf(buffer, "%s_%lu_%lu_%lu_%lu", taskName.c_str(), lx, ly, lz, iter);
  return std::string(buffer);
 }

 inline uint64_t getLocalTaskId(const uint64_t lx, const uint64_t ly, const uint64_t lz) { return lz * ls.y * ls.x + ly * ls.x + lx; }

}; // class Grid

