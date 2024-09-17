testSuite = [ 'examples', 'local' ]

workerSpecific = executable('workerSpecific', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('workerSpecific', abcTasks, args : [ ], suite: testSuite )
endif
