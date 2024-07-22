####### Getting HiCR dependency

HiCRBackends = [ 'host/hwloc', 'host/pthreads' ]
HiCRProject = subproject('hicr', required: true, default_options: [ 'backends=' + ','.join(HiCRBackends)])
HiCRBuildDep = HiCRProject.get_variable('hicrBuildDep')

####### Creating TaskR dependency

# Warning handling option
warningAsErrorFlags=[]
if get_option('compileWarningsAsErrors') == true
  warningAsErrorFlags=[ '-Werror' ]
endif

TaskRBuildCppArgs = [
 '-Wfatal-errors',
  warningAsErrorFlags
 ]

TaskRBuildIncludes = include_directories([
   '../include'
  ])
  
TaskRBuildDep = declare_dependency(
    compile_args: TaskRBuildCppArgs,
    include_directories: TaskRBuildIncludes,
    sources : [ '../source/tasking.cpp' ]
    )


####### Creating TaskR test dependencies

if meson.is_subproject() or get_option('buildTests')
  gtest_dep = dependency('gtest', main: true, required: true)
  
  TaskRTestCppFlags = [
  ]
  
  cpp = meson.get_compiler('cpp')
  if get_option('b_coverage')
    gcov_args = [
      '-fno-inline',
      '-Wno-error=cpp',
      '-DENABLE_COVERAGE',
    ]
    if cpp.get_id().to_lower() == 'gcc'
      gcov_args += [
        '-fno-default-inline',
        '-fno-inline-small-functions',
      ]
    endif
    
    TaskRTestCppFlags += gcov_args
  endif  
  
  TaskRTestDep = declare_dependency(
      compile_args: TaskRTestCppFlags,
      dependencies:  gtest_dep
      )
endif
