testSuite = [ 'examples', 'local', 'simple' ]

threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('threading', threading, args : [ ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('nosv', nosv, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif

if get_option('buildPyTaskR')
	pysimple_cpp = executable('pysimple_cpp', [ 'source/pysimple.cpp'], dependencies: [ TaskRBuildDep ])

	if get_option('buildTests')
		test('pysimple_cpp', pysimple_cpp, args : [ ], suite: testSuite, workdir: pysimple_cpp.path() + '.p' )

		test('pysimple', 
			 py,
			 args : [ 'py_source/main.py' ], 
			 env: ['PYTHONPATH=' + meson.current_source_dir() + ':' + meson.project_build_root() + '/include/pytaskr/'],
			 suite: testSuite, 
			 workdir: meson.current_source_dir())
	endif
endif