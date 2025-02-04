testSuite = [ 'examples', 'local' ]

manyParallel = executable('manyParallel', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('manyParallel', manyParallel, args : [ '2', '100' ], suite: testSuite, workdir: manyParallel.path() + '.p' )
endif
