testSuite = [ 'examples', 'local', 'multiJob' ]

threading = executable('threading', [ 'source/pthreads.cpp', 'source/job1.cpp', 'source/job2.cpp' ], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('threading', threading, args : [ ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/nosv.cpp', 'source/job1.cpp', 'source/job2.cpp' ], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('nosv', nosv, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif

if get_option('buildPyTaskR') and get_option('buildTests')
	test('pyTaskR', 
			py,
			args : [ 'py_source/main.py' ], 
			env: ['PYTHONPATH=' + meson.current_source_dir() + ':' + meson.project_build_root() + '/include/pytaskr/'],
			suite: testSuite, 
			workdir: meson.current_source_dir())
endif