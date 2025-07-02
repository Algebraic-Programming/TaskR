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

#### Testing if the python interface of pytaskr is working

if get_option('buildPyTaskR')
  testSuite = ['tests', 'pyruntime']
	pyruntime = executable('pyruntime_test', [ 'pyruntime_test.cpp'], dependencies: [ TaskRBuildDep ])

	test('pyruntime_test', pyruntime, args : [ ], suite: testSuite, workdir: pyruntime.path() + '.p' )
endif