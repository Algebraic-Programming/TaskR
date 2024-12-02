testSuite = [ 'examples', 'local' ]

fibonacci = executable('fibonacci', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ],  cpp_args : ['-DUSE_INSTRUMENTATION', '-DENABLE_DEBUG']  )

if get_option('buildTests')
	test('fibonacci', fibonacci, args : [ '15' ], suite: testSuite )
endif
