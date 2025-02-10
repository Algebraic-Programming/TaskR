testSuite = [ 'examples', 'distributed' ]

pingPong = executable('pingPong', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

if get_option('buildTests')
	test('pingPong', pingPong, args : [ ], suite: testSuite, workdir: pingPong.path() + '.p' )
endif
