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
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>

#include <taskr/taskr.hpp>
#include <pytaskr/pyruntime.hpp>

namespace py = pybind11;

namespace taskr
{

PYBIND11_MODULE(taskr, m)
{
    m.doc() = "pybind11 plugin for TaskR";
    
    // TaskR's Runtime class
    py::class_<Runtime>(m, "Runtime")
    .def("setTaskCallbackHandler", &Runtime::setTaskCallbackHandler)
    .def("initialize", &Runtime::initialize)
    .def("addTask", &Runtime::addTask, py::keep_alive<1, 2>())
    .def("run", &Runtime::run, py::call_guard<py::gil_scoped_release>())
    .def("await_", &Runtime::await, py::call_guard<py::gil_scoped_release>())
    .def("finalize", &Runtime::finalize);

    // pyTaskR's PyRuntime class
    py::class_<PyRuntime>(m, "taskr")
    .def(py::init<const std::string&, size_t>(), py::arg("backend") = "threading", py::arg("num_workers") = 0)
    .def("get_runtime", &PyRuntime::get_runtime, py::return_value_policy::reference_internal);
    
    // TaskR's Function class
    py::class_<Function>(m, "Function")
    .def(py::init<const function_t>());
    
    // TaskR's Task class
    py::class_<Task>(m, "Task")
    .def(py::init<const label_t, Function*, const workerId_t>(), py::arg("label"), py::arg("fc"), py::arg("workerAffinity") = -1)
    .def("getLabel", &Task::getLabel)
    .def("addDependency", &Task::addDependency);
}

}