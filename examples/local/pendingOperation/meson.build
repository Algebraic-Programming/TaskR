testSuite = [ 'examples', 'local' ]

pendingOperation = executable('pendingOperation', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('pendingOperation', pendingOperation, args : [ ], suite: testSuite, workdir: pendingOperation.path() + '.p' )
endif
