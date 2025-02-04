testSuite = [ 'examples', 'local' ]

mutex = executable('mutexExample', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('mutex', mutex, args : [ ], suite: testSuite, workdir: mutex.path() + '.p' )
endif
