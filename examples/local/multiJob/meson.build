testSuite = [ 'examples', 'local', 'multiJob' ]

if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
	threading = executable('threading', [ 'source/pthreads.cpp', 'source/job1.cpp', 'source/job2.cpp' ], dependencies: [ TaskRBuildDep ] )

	if get_option('buildTests')
		test('threading', threading, args : [ ], suite: testSuite, workdir: threading.path() + '.p' )
	endif
endif

if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	nosv = executable('nosv', [ 'source/nosv.cpp', 'source/job1.cpp', 'source/job2.cpp' ], dependencies: [ TaskRBuildDep ] )

	if get_option('buildTests')
		test('nosv', nosv, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
	endif
endif