testSuite = [ 'examples', 'local', 'conditionVariable' ]

threading_conditionVariableWait = executable('threading_conditionVariableWait', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWait'] )
threading_conditionVariableWaitFor = executable('threading_conditionVariableWaitFor', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitFor'] )
threading_conditionVariableWaitCondition = executable('threading_conditionVariableWaitCondition', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitCondition'] )
threading_conditionVariableWaitForCondition = executable('threading_conditionVariableWaitForCondition', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitForCondition'] )

if get_option('buildTests')
	test('threading_conditionVariableWait', threading_conditionVariableWait, args : [ ], suite: testSuite, workdir: threading_conditionVariableWait.path() + '.p' )
	test('threading_conditionVariableWaitFor', threading_conditionVariableWaitFor, args : [ ], suite: testSuite, workdir: threading_conditionVariableWaitFor.path() + '.p' )
	test('threading_conditionVariableWaitCondition', threading_conditionVariableWaitCondition, args : [ ], suite: testSuite, workdir: threading_conditionVariableWaitCondition.path() + '.p' )
	test('threading_conditionVariableWaitForCondition', threading_conditionVariableWaitForCondition, args : [ ], suite: testSuite, workdir: threading_conditionVariableWaitForCondition.path() + '.p' )
endif

nosv_conditionVariableWait = executable('nosv_conditionVariableWait', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWait'] )
nosv_conditionVariableWaitFor = executable('nosv_conditionVariableWaitFor', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitFor'] )
nosv_conditionVariableWaitCondition = executable('nosv_conditionVariableWaitCondition', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitCondition'] )
nosv_conditionVariableWaitForCondition = executable('nosv_conditionVariableWaitForCondition', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitForCondition'] )

if get_option('buildTests')
	test('nosv_conditionVariableWait', nosv_conditionVariableWait, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWait.path() + '.p' )
	test('nosv_conditionVariableWaitFor', nosv_conditionVariableWaitFor, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWaitFor.path() + '.p' )
	test('nosv_conditionVariableWaitCondition', nosv_conditionVariableWaitCondition, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWaitCondition.path() + '.p' )
	test('nosv_conditionVariableWaitForCondition', nosv_conditionVariableWaitForCondition, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWaitForCondition.path() + '.p' )
endif
