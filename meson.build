project('TaskR', [ 'cpp', 'c'],
  subproject_dir : 'extern',
  default_options : [
  'cpp_std=c++20',
  'buildtype=release'
  ]
)

####### Storage for TaskR dependencies

taskrDependencies = [ ]

####### Getting HiCR dependency (only if not a subproject)

if meson.is_subproject() == false

  # Selecting default HiCR Backends
  HiCRBackends = [ 'hwloc', 'pthreads', 'boost', 'nosv' ]

  # Getting selected distributed engine

  distributedEngine = get_option('distributedEngine')

  # If, selected, adding the dependency for the distributed engine
  if distributedEngine != 'none'
    HiCRBackends += distributedEngine
  endif

  HiCRProject = subproject('hicr', required: true, default_options: [ 'backends=' + ','.join(HiCRBackends), 'frontends=tasking' ])
  HiCRBuildDep = HiCRProject.get_variable('hicrBuildDep')
  taskrDependencies += HiCRBuildDep

endif

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
   'include'
  ])

####### Build Instrumentation using TraCR

if get_option('buildInstrumentation')

  InstrumentationProject = subproject('tracr', required: true)
  InstrumentationBuildDep = InstrumentationProject.get_variable('InstrumentationBuildDep')
  taskrDependencies += InstrumentationBuildDep

  add_project_arguments('-DENABLE_INSTRUMENTATION', language: 'cpp')

  # uncomment this to enable debug prints of TraCR
  # add_project_arguments('-DENABLE_DEBUG', language: 'cpp')
endif

if get_option('buildPythonAPI')
  subdir('include/python_api')
endif

####### Collect the dependencies

TaskRBuildDep = declare_dependency(
    compile_args: TaskRBuildCppArgs,
    include_directories: TaskRBuildIncludes,
    dependencies: taskrDependencies
    )

####### Build test / example targets only if TaskR is being loaded as a subproject

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
