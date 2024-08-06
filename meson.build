project('TaskR', [ 'cpp', 'c'],
  subproject_dir : 'extern',
  default_options : [
  'cpp_std=c++20',
  'buildtype=release'
  ]
)

####### Getting selected distributed engine

distributedEngine = get_option('distributedEngine')

####### Getting HiCR dependency

# Selecting default HiCR Backends
HiCRBackends = [ 'host/hwloc', 'host/pthreads' ]

# If, selected, adding the dependency for the distributed engine
if distributedEngine != 'none'
  HiCRBackends += distributedEngine
endif


HiCRProject = subproject('hicr', required: true, default_options: [ 'backends=' + ','.join(HiCRBackends), 'frontends=tasking' ])
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

# Handling distributed engine options
if distributedEngine == 'mpi'
 TaskRBuildCppArgs += '-D_TASKR_DISTRIBUTED_ENGINE_MPI'
endif

if distributedEngine == 'lpf'
 TaskRBuildCppArgs += '-D_TASKR_DISTRIBUTED_ENGINE_LPF'
endif

TaskRBuildIncludes = include_directories([
   '../include'
  ])
  
TaskRBuildDep = declare_dependency(
    compile_args: TaskRBuildCppArgs,
    include_directories: TaskRBuildIncludes,
    dependencies: HiCRBuildDep
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

####### Build test / example targets only if HiCR is being loaded as a subproject

if meson.is_subproject() == false

  # Build example targets
  if get_option('buildExamples')
  subdir('examples')
  endif

  # Build test targets
  if get_option('buildTests')
  subdir('tests')
  endif

endif
