testSuite = [ 'examples', 'tasking' ]

resourceList = executable('resourceList', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ])

if get_option('buildTests')
	test('resourceList', resourceList, args : [ '4', '100', '0', '1', '2', '3' ], suite: testSuite)
endif

