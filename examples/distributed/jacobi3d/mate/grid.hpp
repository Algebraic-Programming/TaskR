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
	int localId;
	D3 lPos;
};

struct SubGrid {
 Neighbor East, West, North, South, Up, Down;
 D3 lPos;
 D3 lStart;
 D3 lEnd;
};

class Grid
{
 public:

 // Configuration
 int processId; // Id for the current global process
 size_t nIters; // Number of iterations
 size_t N; // Grid ls per side (N)
 int gDepth; // Ghost cell depth

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

 // MPI Datatypes
 MPI_Datatype faceX_type;
 MPI_Datatype faceY_type;
 MPI_Datatype faceX_subtype;
 MPI_Datatype faceZ_type;

 // Local subgrid information
 std::vector<SubGrid> subgrids;

 public:

 Grid(const int processId, const size_t N, const size_t nIters, const size_t gDepth, const D3& pt, const D3& lt);
 bool initialize();
 void finalize();
 void solve();
 void reset();
 double calculateResidual();
};

