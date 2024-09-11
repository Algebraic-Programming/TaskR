testSuite = [ 'examples', 'distributed' ]

jacobi = executable('jacobi', [ 'source/jacobi.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ] )

if get_option('buildTests')
	test('jacobi', jacobi, args : [ ], suite: testSuite )
endif
