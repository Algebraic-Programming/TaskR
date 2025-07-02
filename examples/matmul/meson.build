testSuite = [ 'examples', 'matmul' ]

if get_option('buildPyTaskR') and get_option('buildTests')
	py.extension_module('cpp_matmul',
		['python/matmul.cpp'],
		install: true,
		dependencies : [TaskRBuildDep, pybind11_dep],
	)

	test('pyTaskR', 
		py,
		args : [ 'python/main.py' ],
		is_parallel : false, 
		env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/:' + meson.project_build_root() + '/examples/matmul/'],
		suite: testSuite, 
		workdir: meson.current_source_dir())
endif