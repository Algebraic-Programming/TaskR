testSuite = [ 'examples', 'jacobi3d' ]

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