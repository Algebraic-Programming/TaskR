testSuite = [ 'examples', 'distributed' ]

pingPong = executable('pingPong', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('pingPong', pingPong, args : [ ], suite: testSuite )
endif
