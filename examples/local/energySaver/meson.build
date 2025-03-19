testSuite = [ 'examples', 'local', 'energySaver' ]

threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('threading', threading, args : [ '3', '1', '100' ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('nosv', nosv, args : [ '3', '1', '100' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif