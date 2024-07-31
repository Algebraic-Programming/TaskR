testSuite = [ 'examples' ]

getCurrentTask = executable('getCurrentTask', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ])

if get_option('buildTests')
    test('getCurrentTask', getCurrentTask, args : [ ], suite: testSuite)
endif
