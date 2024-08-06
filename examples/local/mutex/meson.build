testSuite = [ 'examples' ]

mutex = executable('mutexExample', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('mutex', mutex, args : [ ], suite: testSuite )
endif
