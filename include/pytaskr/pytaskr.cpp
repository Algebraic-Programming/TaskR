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
#include <pybind11/functional.h> // std::function
#include <pybind11/stl.h>        // std::set

#include <taskr/taskr.hpp>
#include <pytaskr/pyruntime.hpp>

namespace py = pybind11;

namespace taskr
{

/**
 * Pybind11 module for binding taskr stuff
 */
PYBIND11_MODULE(taskr, m)
{
  m.doc() = "pybind11 plugin for TaskR";

  py::enum_<backend_t>(m, "HiCRBackend").value("nosv", backend_t::nosv).value("threading", backend_t::threading).export_values();

  // pyTaskR's PyRuntime class
  py::class_<PyRuntime>(m, "taskr")
    .def(py::init<const backend_t &, size_t>(), py::arg("backend") = backend_t::nosv, py::arg("num_workers") = 0)
    .def(py::init<const backend_t &, const std::set<int> &>(), py::arg("backend") = backend_t::nosv, py::arg("workersSet"))
    .def("get_runtime", &PyRuntime::get_runtime, py::return_value_policy::reference_internal)
    .def("get_num_workers", &PyRuntime::get_num_workers);

  // TaskR's Runtime class
  py::class_<Runtime>(m, "Runtime")
    .def("setTaskCallbackHandler", &Runtime::setTaskCallbackHandler)
    .def("setServiceWorkerCallbackHandler", &Runtime::setServiceWorkerCallbackHandler)
    .def("setTaskWorkerCallbackHandler", &Runtime::setTaskWorkerCallbackHandler)
    .def("initialize", &Runtime::initialize)
    .def("addTask", &Runtime::addTask, py::keep_alive<1, 2>()) // keep_alive as the task should be alive until runtime's destructor
    .def("resumeTask", &Runtime::resumeTask)
    .def("run", &Runtime::run, py::call_guard<py::gil_scoped_release>())
    .def("await_", &Runtime::await, py::call_guard<py::gil_scoped_release>()) // Release GIL is important otherwise non-finished tasks are getting blocked
    .def("finalize", &Runtime::finalize)
    .def("setFinishedTask", &Runtime::setFinishedTask)
    .def("addService", &Runtime::addService);

  // TaskR's Function class
  py::class_<Function>(m, "Function").def(py::init<const function_t>());

  // TaskR's Task class
  py::class_<Task>(m, "Task")
    .def(py::init<Function *, const workerId_t>(), py::arg("fc"), py::arg("workerAffinity") = -1)
    .def(py::init<const label_t, Function *, const workerId_t>(), py::arg("label"), py::arg("fc"), py::arg("workerAffinity") = -1)
    .def("getLabel", &Task::getLabel)
    .def("setLabel", &Task::setLabel)
    .def("getWorkerAffinity", &Task::getWorkerAffinity)
    .def("setWorkerAffinity", &Task::setWorkerAffinity)
    .def("addDependency", &Task::addDependency)
    .def("getDependencyCount", &Task::getDependencyCount)
    .def("incrementDependencyCount", &Task::incrementDependencyCount)
    .def("decrementDependencyCount", &Task::decrementDependencyCount)
    .def("addOutputDependency", &Task::addOutputDependency)
    .def("getOutputDependencies", &Task::getOutputDependencies)
    .def("addPendingOperation", &Task::addPendingOperation)
    .def("getPendingOperations", &Task::getPendingOperations)
    .def("suspend", &Task::suspend, py::call_guard<py::gil_scoped_release>());

  py::enum_<Task::callback_t>(m, "TaskCallback")
    .value("onTaskExecute", Task::callback_t::onTaskExecute)
    .value("onTaskSuspend", Task::callback_t::onTaskSuspend)
    .value("onTaskFinish", Task::callback_t::onTaskFinish)
    .value("onTaskSync", Task::callback_t::onTaskSync)
    .export_values();

  // TaskR's Mutex class
  py::class_<Mutex>(m, "Mutex").def(py::init<>()).def("lock", &Mutex::lock).def("unlock", &Mutex::unlock).def("ownsLock", &Mutex::ownsLock).def("trylock", &Mutex::trylock);

  // TaskR's ConditionVariable class
  py::class_<ConditionVariable>(m, "ConditionVariable")
    .def(py::init<>())
    .def("wait", py::overload_cast<Task *, Mutex &>(&ConditionVariable::wait), py::call_guard<py::gil_scoped_release>(), "cv wait")
    .def("wait", py::overload_cast<Task *, Mutex &, const std::function<bool(void)> &>(&ConditionVariable::wait), py::call_guard<py::gil_scoped_release>(), "cv wait with condition")
    .def("waitFor",
         py::overload_cast<Task *, Mutex &, const std::function<bool(void)> &, size_t>(&ConditionVariable::waitFor),
         py::call_guard<py::gil_scoped_release>(),
         "cv waitFor with condition")
    .def("waitFor", py::overload_cast<Task *, Mutex &, size_t>(&ConditionVariable::waitFor), py::call_guard<py::gil_scoped_release>(), "cv waitFor")
    .def("notifyOne", &ConditionVariable::notifyOne) // Not sure if I need to release the GIL here as this function also uses the intern mutex lock
    .def("notifyAll", &ConditionVariable::notifyAll) // Same here
    .def("getWaitingTaskCount", &ConditionVariable::getWaitingTaskCount);
}

} // namespace taskr