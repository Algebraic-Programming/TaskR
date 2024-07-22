testSuite = [ 'examples', 'tasking' ]

fibonacci = executable('fibonacci', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ] )

if get_option('buildTests')
	test('fibonacci', fibonacci, args : [ ], suite: testSuite )
endif
