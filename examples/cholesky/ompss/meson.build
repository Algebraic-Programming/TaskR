testSuite = [ 'examples' ]

choleskyompssDep = declare_dependency(
    compile_args: ['-fompss-2'],
    link_args: ['-fompss-2'],
    dependencies: [ choleskyDep, 
    dependency('openmp', required: true)]
)

ompss = executable('ompss', [ 'main.cpp'], include_directories: ['.'], dependencies: choleskyompssDep )
