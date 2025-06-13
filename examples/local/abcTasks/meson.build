testSuite = [ 'examples', 'local', 'abcTasks' ]

if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
	threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ])

	if get_option('buildTests')
		test('threading', threading, args : [ ], suite: testSuite, workdir: threading.path() + '.p' )
	endif
endif

if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	nosv = executable('nosv', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('nosv', nosv, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p')
endif

if get_option('buildPyTaskR') and get_option('buildTests')
	test('pyTaskR', 
		 py,
		 args : [ 'py_source/main.py' ],
		 is_parallel : false, 
		 env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/'],
		 suite: testSuite, 
		 workdir: meson.current_source_dir())
endif