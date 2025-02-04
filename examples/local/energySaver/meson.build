testSuite = [ 'examples', 'local' ]

energySaver = executable('energySaver', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('energySaver', energySaver, args : [ '3', '1', '100' ], suite: testSuite, workdir: energySaver.path() + '.p' )
endif

