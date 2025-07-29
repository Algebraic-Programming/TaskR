testSuite = [ 'examples', 'jacobi3d' ]

# Handling distributed engine options
if distributedEngine == 'mpi'
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_MPI'
	mpirunExecutable = HiCRProject.get_variable('mpirunExecutable')
elif distributedEngine == 'lpf'
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_LPF'
	lpfrunExecutable = HiCRProject.get_variable('lpfrunExecutable')
elif distributedEngine == 'none'
	TaskRDistributedCppFlag = '-D_TASKR_DISTRIBUTED_ENGINE_NONE'
endif

if distributedEngine == 'mpi'
	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		if get_option('buildTests')
			test('threading', mpirunExecutable, args : [ '-n', '2', '--oversubscribe', threading.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], suite: testSuite, workdir: threading.path() + '.p' )
		endif
	endif

	# nOS-V backend with MPI not yet supported (we run without mpirun)
	if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
		nosv = executable('nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

		if get_option('buildTests')
			test('nosv', nosv, args : [ '-n', '64', '-i', '10' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
		endif
	endif
	
elif distributedEngine == 'lpf'
	if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
		threading = executable('threading', [ 'source/pthreads.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )
		if get_option('buildTests')
			test('threading', lpfrunExecutable, args : [ '-n', '2', threading.full_path(), '-px', '1', '-py', '1', '-pz', '2', '-lx', '1', '-ly', '2', '-lz', '2', '-n', '64', '-i', '10'], suite: testSuite, workdir: threading.path() + '.p' )
		endif
	endif

	# nOS-V backend with LPF not yet supported (we run without lpfrun)
	# if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	# 	nosv = executable('nosv', [ 'source/nosv.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

	# 	if get_option('buildTests')
	# 		test('nosv', nosv, args : [ '-n', '64', '-i', '10' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
	# 	endif
	# endif
elif distributedEngine == 'none' # Atm these segfault
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