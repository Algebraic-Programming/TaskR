testSuite = [ 'examples', 'conditionVariable' ]

if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
	threading_conditionVariableWait = executable('threading_conditionVariableWait', [ 'cpp/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWait'] )
	threading_conditionVariableWaitFor = executable('threading_conditionVariableWaitFor', [ 'cpp/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitFor'] )
	threading_conditionVariableWaitCondition = executable('threading_conditionVariableWaitCondition', [ 'cpp/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitCondition'] )
	threading_conditionVariableWaitForCondition = executable('threading_conditionVariableWaitForCondition', [ 'cpp/pthreads.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitForCondition'] )

	if get_option('buildTests')
		test('threading_conditionVariableWait', threading_conditionVariableWait, args : [ ], suite: testSuite, workdir: threading_conditionVariableWait.full_path() + '.p' )
		test('threading_conditionVariableWaitFor', threading_conditionVariableWaitFor, args : [ ], suite: testSuite, workdir: threading_conditionVariableWaitFor.full_path() + '.p' )
		test('threading_conditionVariableWaitCondition', threading_conditionVariableWaitCondition, args : [ ], suite: testSuite, workdir: threading_conditionVariableWaitCondition.full_path() + '.p' )
		test('threading_conditionVariableWaitForCondition', threading_conditionVariableWaitForCondition, args : [ ], suite: testSuite, workdir: threading_conditionVariableWaitForCondition.full_path() + '.p' )
	endif
endif

if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	nosv_conditionVariableWait = executable('nosv_conditionVariableWait', [ 'cpp/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWait'] )
	nosv_conditionVariableWaitFor = executable('nosv_conditionVariableWaitFor', [ 'cpp/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitFor'] )
	nosv_conditionVariableWaitCondition = executable('nosv_conditionVariableWaitCondition', [ 'cpp/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitCondition'] )
	nosv_conditionVariableWaitForCondition = executable('nosv_conditionVariableWaitForCondition', [ 'cpp/nosv.cpp'], dependencies: [ TaskRBuildDep ], cpp_args: [ '-D__TEST_FUNCTION_=conditionVariableWaitForCondition'] )

	if get_option('buildTests')
		test('nosv_conditionVariableWait', nosv_conditionVariableWait, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWait.full_path() + '.p' )
		test('nosv_conditionVariableWaitFor', nosv_conditionVariableWaitFor, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWaitFor.full_path() + '.p' )
		test('nosv_conditionVariableWaitCondition', nosv_conditionVariableWaitCondition, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWaitCondition.full_path() + '.p' )
		test('nosv_conditionVariableWaitForCondition', nosv_conditionVariableWaitForCondition, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv_conditionVariableWaitForCondition.full_path() + '.p' )
	endif
endif

if get_option('buildPyTaskR') and get_option('buildTests')
	test('pyTaskR_conditionVariableWait', 
		 py,
		 args : [ 'python/main.py' ],
		 is_parallel : false, 
		 env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/', '__TEST_FUNCTION_=conditionVariableWait'],
		 suite: testSuite, 
		 workdir: meson.current_source_dir())

	test('pyTaskR_conditionVariableWaitFor', 
		 py,
		 args : [ 'python/main.py' ],
		 is_parallel : false, 
		 env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/', '__TEST_FUNCTION_=conditionVariableWaitFor'],
		 suite: testSuite, 
		 workdir: meson.current_source_dir())

	test('pyTaskR_conditionVariableWaitCondition', 
		 py,
		 args : [ 'python/main.py' ],
		 is_parallel : false, 
		 env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/', '__TEST_FUNCTION_=conditionVariableWaitCondition'],
		 suite: testSuite, 
		 workdir: meson.current_source_dir())

	test('pyTaskR_conditionVariableWaitForCondition', 
		 py,
		 args : [ 'python/main.py' ],
		 is_parallel : false, 
		 env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/', '__TEST_FUNCTION_=conditionVariableWaitForCondition'],
		 suite: testSuite, 
		 workdir: meson.current_source_dir())
endif