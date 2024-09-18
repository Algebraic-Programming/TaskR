testSuite = [ 'examples', 'distributed' ]

jacobi = executable('jacobi', [ 'source/jacobi.cpp', 'source/grid.cpp' ], dependencies: [ TaskRBuildDep ], cpp_args: [ TaskRDistributedCppFlag ] )

if get_option('buildTests')
	test('jacobi', jacobi, args : [ ], suite: testSuite )
endif
