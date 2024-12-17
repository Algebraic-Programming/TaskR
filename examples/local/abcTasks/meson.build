testSuite = [ 'examples', 'local' ]

abcTasks = executable('abcTasks', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('abcTasks', abcTasks, args : [ ], suite: testSuite )
endif
