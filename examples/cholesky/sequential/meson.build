testSuite = [ 'examples' ]

choleskySequentialDep = declare_dependency(
    dependencies: [ choleskyDep, dependency('openmp', required: true)]
)

sequential = executable('sequential', [ 'main.cpp'], include_directories: ['.'], dependencies: choleskySequentialDep )
