testSuite = [ 'examples' ]

abcTasks = executable('abcTasks', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ] )

if get_option('buildTests')
	test('abcTasks', abcTasks, args : [ ], suite: testSuite )
endif
