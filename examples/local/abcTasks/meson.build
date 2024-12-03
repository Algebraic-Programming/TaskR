testSuite = [ 'examples', 'local' ]

abcTasks = executable('abcTasks', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ],  cpp_args : cpp_args)

if get_option('buildTests')
	test('abcTasks', abcTasks, args : [ ], suite: testSuite )
endif
