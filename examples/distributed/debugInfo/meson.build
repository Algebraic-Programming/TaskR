testSuite = [ 'examples', 'distributed' ]

debugInfo = executable('debugInfo', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('debugInfo', debugInfo, args : [ ], suite: testSuite )
endif
