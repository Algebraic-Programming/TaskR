# pybind11 stuff with the compiler flags
# Manually compile like this:
# c++ -O3 -Wall -shared -std=c++11 -fPIC $(python3-config --includes) -Iextern/pybind11/include example.cpp -o example$(python3-config --extension-suffix)

py = import('python').find_installation(pure: false)
pybind11_dep = dependency('pybind11', required: true)

py.extension_module('taskr',
    ['pytaskr.cpp'],
    install: true,
    dependencies : [TaskRBuildDep, pybind11_dep],
)