testSuite = [ 'examples', 'local', 'simple' ]

threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('threading', threading, args : [ ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('nosv', nosv, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif

if get_option('buildTests') and get_option('buildPyTaskR')
	pyruntime = executable('pyruntime', [ 'source/pyruntime.cpp'], dependencies: [ TaskRBuildDep ])

	test('pyruntime', pyruntime, args : [ ], suite: testSuite, workdir: pyruntime.path() + '.p' )

	# python_script = find_program('py_source/main.py', required: true)
	# test('python_script', python_script, args : [ ], suite: testSuite)
endif