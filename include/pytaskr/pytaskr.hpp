/*
 *   Copyright 2025 Huawei Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <taskr/taskr.hpp>
#include <pytaskr/pyruntime.hpp>

/*
#include <pybind11/pybind11.h>


PYBIND11_MODULE(taskr, m)
{
    m.doc() = "pybind11 plugin for TaskR";
    
    // TaskR's Runtime class
    py::class_<PyRuntime>(m, "PyRuntime")
    .def(py::init<const std::string&, const int&>())
    .def("setTaskCallbackHandler", &Runtime::setTaskCallbackHandler)
    .def("initialize", &Runtime::initialize)
    .def("addTask", &Runtime::addTask)
    .def("run", &Runtime::run)
    .def("await_", &Runtime::await)
    .def("finalize", &Runtime::finalize)
    
    // TaskR's Function class
    py::class_<Function>(m, "Function")
    .def(py::init<const function_t>())
    
    // TaskR's Task class
    py::class_<Task>(m, "Task")
    .def(py::init<const label_t, Function>())
}
*/