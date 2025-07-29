# pybind11 stuff with the compiler flags
# Manually compile like this:
# c++ -O3 -Wall -shared -std=c++11 -fPIC $(python3-config --includes) -Iextern/pybind11/include example.cpp -o example$(python3-config --extension-suffix)

python_mod = import('python')

py = python_mod.find_installation('/usr/bin/python3.13-nogil', pure: false)

if not py.found()
  py = python_mod.find_installation('/usr/bin/python3', pure: false)
endif

pybind11_dep = dependency('pybind11', required: true)

message('Python binary: ' + py.path())
message('Python install dir: ' + py.get_install_dir())

py.extension_module('taskr',
    ['pytaskr.cpp'],
    install: true,
    install_dir: py.get_install_dir(),
    dependencies : [TaskRBuildDep, pybind11_dep],
)