testSuite = [ 'examples', 'local' ]

suspend = executable('suspend', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('suspend', suspend, args : [ '2', '100' ], suite: testSuite )
endif
