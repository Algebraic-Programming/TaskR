testSuite = [ 'examples', 'local', 'mutex' ]

threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('threading', threading, args : [ ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('nosv', nosv, args : [ ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif