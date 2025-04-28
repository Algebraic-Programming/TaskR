testSuite = [ 'examples', 'local', 'cholesky' ]

choleskyDep = declare_dependency(
    dependencies: [ TaskRBuildDep, dependency('openblas', required: true) ],
    include_directories: ['.'],
    compile_args: [ '-ffast-math','-Werror' ]
)

threading = executable('threading', [ 'source/pthreads.cpp'],  include_directories: include_directories(['source']), dependencies: [ choleskyDep] )

if get_option('buildTests')
	test('threading', threading, args: [ '384', '24', '0', '0' ], suite: testSuite, workdir: threading.path() + '.p' )
endif

nosv = executable('nosv', [ 'source/nosv.cpp'],  include_directories: include_directories(['source']), dependencies: [ choleskyDep] )

if get_option('buildTests')
	test('nosv', nosv, args: [ '384', '24', '0', '0' ], is_parallel : false, suite: testSuite, workdir: nosv.path() + '.p' )
endif
