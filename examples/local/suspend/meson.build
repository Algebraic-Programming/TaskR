testSuite = [ 'examples', 'local', 'suspend' ]

if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
	threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ] )

	if get_option('buildTests')
		test('threading', threading, args : [ '2', '100' ], suite: testSuite, workdir: threading.path() + '.p' )
	endif
endif

if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	nosv = executable('nosv', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ] )

	if get_option('buildTests')
		test('nosv', nosv, args : [ '2', '100' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
	endif
endif