testSuite = [ 'examples', 'jacobi3d' ]

if distributedEngine == 'mpi'
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_MPI'

	mpirunExecutable = HiCRProject.get_variable('mpirunExecutable')

	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		
		if get_option('buildTests')
			test('threading', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', threading.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], suite: testSuite, workdir: threading.path() + '.p' )
		endif
	endif

	# TODO: deadlocking (even for mpirun -n 1)
	if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
		nosv = executable('nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		
		if get_option('buildTests')
			test('nosv', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', nosv.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
		endif
	endif
	
elif distributedEngine == 'lpf'
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_LPF'

	mpirunExecutable = HiCRProject.get_variable('mpirunExecutable')

	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		
		if get_option('buildTests')
			test('threading', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', 'env', 'LPF_ENGINE=zero', threading.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], suite: testSuite, workdir: threading.path() + '.p' )
		endif
	endif

	
	if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
		nosv = executable('nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

		if get_option('buildTests')
			test('nosv', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', 'env', 'LPF_ENGINE=zero', nosv.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
		endif
	endif
elif distributedEngine == 'none' # Atm these are segfaulting (TODO fix!)
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_NONE'

	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

		if get_option('buildTests')
			test('threading', threading, args : [ '-n', '64', '-i', '10' ], suite: testSuite, workdir: threading.path() + '.p' )
		endif
	endif

	if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
		nosv = executable('nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

		if get_option('buildTests')
			test('nosv', nosv, args : [ '-n', '64', '-i', '10' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
		endif
	endif
endif