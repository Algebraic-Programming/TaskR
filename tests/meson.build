testSuite = [ 'tests', 'frontends', 'tasking' ]

test('task',       executable('task',       'task.cpp',       dependencies: [TaskRTestDep, HiCRBuildDep, TaskRBuildDep] ), protocol: 'gtest', suite: testSuite )
test('worker',     executable('worker',     'worker.cpp',     dependencies: [TaskRTestDep, HiCRBuildDep, TaskRBuildDep] ), protocol: 'gtest', suite: testSuite )
test('dispatcher', executable('dispatcher', 'dispatcher.cpp', dependencies: [TaskRTestDep, HiCRBuildDep, TaskRBuildDep] ), protocol: 'gtest', suite: testSuite )
