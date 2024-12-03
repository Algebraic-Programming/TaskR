testSuite = [ 'examples', 'local' ]

fibonacci = executable('fibonacci', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ],  cpp_args : cpp_args)

if get_option('buildTests')
	test('fibonacci', fibonacci, args : [ '15' ], suite: testSuite )
endif
