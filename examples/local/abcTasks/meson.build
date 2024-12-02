testSuite = [ 'examples', 'local' ]

abcTasks = executable('abcTasks', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ],  cpp_args : ['-DUSE_INSTRUMENTATION', '-DENABLE_DEBUG'] )

if get_option('buildTests')
	test('abcTasks', abcTasks, args : [ ], suite: testSuite )
endif
