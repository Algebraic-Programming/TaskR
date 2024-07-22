testSuite = [ 'examples', 'tasking' ]

energySaver = executable('energySaver', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ])

if get_option('buildTests')
	test('energySaver', energySaver, args : [ '10', '1', '100' ], suite: testSuite)
endif

