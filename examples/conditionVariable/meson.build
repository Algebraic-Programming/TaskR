testSuite = [ 'examples' ]

conditionVariable = executable('conditionVariableExample', [ 'source/main.cpp'], dependencies: [ HiCRBuildDep, TaskRBuildDep ] )

if get_option('buildTests')
	test('conditionVariable', conditionVariable, args : [ ], suite: testSuite )
endif
