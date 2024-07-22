project('TaskR', [ 'cpp', 'c'],
  subproject_dir : 'extern',
  default_options : [
  'cpp_std=c++20',
  'buildtype=release'
  ]
)

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
   'include'
  ])
  
TaskRBuildDep = declare_dependency(
    compile_args: TaskRBuildCppArgs,
    include_directories: TaskRBuildIncludes,
    )

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
