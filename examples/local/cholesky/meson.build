testSuite = [ 'examples', 'local' ]

choleskyDep = declare_dependency(
    dependencies: [ TaskRBuildDep, dependency('openblas', required: true, version: '0.3.28') ],
    include_directories: ['.'],
    compile_args: [ '-ffast-math','-Werror' ]
)

cholesky = executable('cholesky', [ 'source/main.cpp'],  include_directories: include_directories(['source']), dependencies: [ choleskyDep] )

if get_option('buildTests')
	test('cholesky', cholesky, args: [ '384', '24', '0', '0' ], suite: testSuite )
endif
