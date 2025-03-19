
# Handling distributed engine options
if distributedEngine == 'mpi'
TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_MPI'
endif

if distributedEngine == 'lpf'
TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_LPF'
endif

if distributedEngine == 'none'
TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_NONE'
endif

subdir('jacobi3d')