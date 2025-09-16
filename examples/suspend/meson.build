testSuite = [ 'examples', 'suspend' ]

if 'boost' in get_option('executionStateType') and 'pthreads' in get_option('processingUnitType')
	threading = executable('threading', [ 'cpp/pthreads.cpp'], dependencies: [ TaskRBuildDep ] )

	if get_option('buildTests')
		test('threading', threading, args : [ '2', '100' ], suite: testSuite, workdir: threading.full_path() + '.p' )
	endif
endif

if 'nosv' in get_option('executionStateType') and 'nosv' in get_option('processingUnitType')
	nosv = executable('nosv', [ 'cpp/nosv.cpp'], dependencies: [ TaskRBuildDep ] )

	if get_option('buildTests')
		test('nosv', nosv, args : [ '2', '100' ], is_parallel : false, suite: testSuite, workdir: nosv.full_path() + '.p' )
	endif
endif

if get_option('buildPyTaskR') and get_option('buildTests')
	test('pyTaskR', 
			py,
			args : [ 'python/main.py' ],
			is_parallel : false, 
			env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/'],
			suite: testSuite, 
			workdir: meson.current_source_dir())
endif