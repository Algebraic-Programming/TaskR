testSuite = [ 'examples' ]

multiJob = executable('multiJob', [ 'source/main.cpp', 'source/job1.cpp', 'source/job2.cpp' ], dependencies: [ HiCRBuildDep, TaskRBuildDep ] )

if get_option('buildTests')
	test('multiJob', multiJob, args : [ ], suite: testSuite )
endif
