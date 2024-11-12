testSuite = [ 'examples', 'local', 'conditionVariable' ]

conditionVariableWait = executable('conditionVariableWait', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWait'] )
conditionVariableWaitFor = executable('conditionVariableWaitFor', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitFor'] )
conditionVariableWaitCondition = executable('conditionVariableWaitCondition', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitCondition'] )
conditionVariableWaitForCondition = executable('conditionVariableWaitForCondition', [ 'source/main.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitForCondition'] )

if get_option('buildTests')
	test('conditionVariableWait', conditionVariableWait, args : [ ], suite: testSuite )
	test('conditionVariableWaitFor', conditionVariableWaitFor, args : [ ], suite: testSuite )
	test('conditionVariableWaitCondition', conditionVariableWaitCondition, args : [ ], suite: testSuite )
	test('conditionVariableWaitForCondition', conditionVariableWaitForCondition, args : [ ], suite: testSuite )
endif
