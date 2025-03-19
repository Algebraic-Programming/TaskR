testSuite = [ 'examples', 'local', 'fibonacci' ]

threading = executable('threading', [ 'source/pthreads.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('threading', threading, args : [ '15' ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/nosv.cpp'], dependencies: [ TaskRBuildDep ])

if get_option('buildTests')
	test('nosv', nosv, args : [ '15' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif