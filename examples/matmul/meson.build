testSuite = [ 'examples', 'matmul' ]

if get_option('buildPyTaskR') and get_option('buildTests')
	test('pyTaskR', 
		py,
		args : [ 'python/main.py' ],
		is_parallel : false, 
		env: ['PYTHONPATH=' + meson.project_build_root() + '/include/pytaskr/'],
		suite: testSuite, 
		workdir: meson.current_source_dir())
endif