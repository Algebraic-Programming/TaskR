testSuite = [ 'examples', 'distributed' ]

if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
	pingPong = executable('pingPong', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

	if get_option('buildTests')
		test('pingPong', pingPong, args : [ ], suite: testSuite, workdir: pingPong.path() + '.p' )
	endif
endif
