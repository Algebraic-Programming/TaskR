#include <vector>
#include <cstdint>
#include <string>
#include <memory>
#include <taskr/taskr.hpp>
#include <hicr/core/L0/executionUnit.hpp>
#include <hicr/frontends/channel/fixedSize/spsc/consumer.hpp>
#include <hicr/frontends/channel/fixedSize/spsc/producer.hpp>

const int BLOCKZ=96;
const int BLOCKY=64;

enum commType {
	  REMOTE=0,
      LOCAL,
	  BOUNDARY
};

struct D3 {
	ssize_t x;
	ssize_t y;
	ssize_t z;
};

struct Neighbor {
	commType type;
    ssize_t processId;
	ssize_t localId;
	D3 lPos;
};

struct SubGrid {
 Neighbor East, West, North, South, Up, Down;
 D3 lPos;
 D3 lStart;
 D3 lEnd;

 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> upRecvChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> downRecvChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> eastRecvChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> westRecvChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> northRecvChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Consumer> southRecvChannel;

 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> upSendChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> downSendChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> eastSendChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> westSendChannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> northSendhannel;
 std::unique_ptr<HiCR::channel::fixedSize::SPSC::Producer> southSendChannel;
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
 ssize_t*** globalProcessMapping; // MPI process mapping
 ssize_t localRankCount;

 // Local subgrid information
 ssize_t*** localSubGridMapping;
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

 // Execution unit definitions
 std::shared_ptr<HiCR::L0::ExecutionUnit> resetFc;
 std::shared_ptr<HiCR::L0::ExecutionUnit> computeFc;
 std::shared_ptr<HiCR::L0::ExecutionUnit> receiveFc;
 std::shared_ptr<HiCR::L0::ExecutionUnit> unpackFc;
 std::shared_ptr<HiCR::L0::ExecutionUnit> packFc;
 std::shared_ptr<HiCR::L0::ExecutionUnit> sendFc;

 Grid(taskr::Runtime* const taskr, const int processId, const size_t N, const size_t nIters, const size_t gDepth, const D3& pt, const D3& lt);
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

 static inline size_t encodeTaskName(const std::string& taskName, const uint64_t lx, const uint64_t ly, const uint64_t lz, const uint64_t iter)
 {
  char buffer[512];
  sprintf(buffer, "%s_%lu_%lu_%lu_%lu", taskName.c_str(), lx, ly, lz, iter);
  const std::hash<std::string> hasher;
  const auto hashResult = hasher(buffer);
  return hashResult;
 }

 inline uint64_t getLocalTaskId(const uint64_t lx, const uint64_t ly, const uint64_t lz) { return lz * ls.y * ls.x + ly * ls.x + lx; }

 taskr::Runtime* const _taskr;

}; // class Grid

