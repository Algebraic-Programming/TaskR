testSuite = [ 'examples', 'tasking' ]

mutex = executable('mutexExample', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ] )

if get_option('buildTests')
	test('mutex', mutex, args : [ ], suite: testSuite )
endif
