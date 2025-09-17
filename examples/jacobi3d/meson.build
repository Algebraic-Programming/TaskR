testSuite = [ 'examples', 'jacobi3d' ]

if 'mpi' in distributedEngines
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_MPI'

	mpirunExecutable = HiCRProject.get_variable('mpirunExecutable')

	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('mpi-threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		
		if get_option('buildTests')
			test('mpi-threading', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', threading.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], suite: testSuite + ['mpi'], workdir: threading.full_path() + '.p' )
		endif
	endif

	if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
		nosv = executable('mpi-nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		
		if get_option('buildTests')
			test('mpi-nosv', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', nosv.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], is_parallel : false, suite: testSuite + ['mpi'], workdir: nosv.full_path() + '.p' )
		endif
	endif
endif
	
if 'lpf' in distributedEngines
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_LPF'

	mpirunExecutable = find_program('mpirun', '/usr/bin/mpirun', '/usr/local/bin/mpirun', required : true)

	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('lpf-threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		
		if get_option('buildTests')
			test('lpf-threading', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', 'env', 'LPF_ENGINE=zero', threading.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], suite: testSuite + ['lpf'], workdir: threading.full_path() + '.p' )
		endif
	endif

	
	# TODO: Actually works but with an error message of MPI not finalizing properly
	# if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	# 	nosv = executable('lpf-nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

	# 	if get_option('buildTests')
	# 		test('lpf-nosv', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', 'env', 'LPF_ENGINE=zero', nosv.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], is_parallel : false, suite: testSuite + ['lpf'], workdir: nosv.full_path() + '.p' )
	# 	endif
	# endif
endif