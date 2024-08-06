testSuite = [ 'examples' ]

fibonacci = executable('fibonacci', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('fibonacci', fibonacci, args : [ '15' ], suite: testSuite )
endif
