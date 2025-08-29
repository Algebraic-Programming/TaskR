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

  // pyTaskR's PyRuntime class (Wrapper class for the TaskR Runtime)
  py::class_<PyRuntime>(m, "create")
    .def(py::init<const std::string &, size_t>(), py::arg("backend") = "nosv", py::arg("num_workers") = 0)
    .def(py::init<const std::string &, const std::set<int> &>(), py::arg("backend") = "nosv", py::arg("workersSet"))
    .def("get_num_workers", &PyRuntime::get_num_workers)
    .def("setTaskCallbackHandler", &PyRuntime::setTaskCallbackHandler)
    .def("setServiceWorkerCallbackHandler", &PyRuntime::setServiceWorkerCallbackHandler)
    .def("setTaskWorkerCallbackHandler", &PyRuntime::setTaskWorkerCallbackHandler)
    .def("addTask", &PyRuntime::addTask, py::keep_alive<1, 2>(), py::arg("task")) // keep_alive as the task should be alive until runtime's destructor
    .def("resumeTask", &PyRuntime::resumeTask)
    .def("initialize", &PyRuntime::initialize)
    .def("run", &PyRuntime::run, py::call_guard<py::gil_scoped_release>())
    .def("wait", &PyRuntime::await, py::call_guard<py::gil_scoped_release>()) // Release GIL is important otherwise non-finished tasks are getting blocked
    .def("finalize", &PyRuntime::finalize)
    .def("setFinishedTask", &PyRuntime::setFinishedTask)
    .def("addService", &PyRuntime::addService);

  // TaskR's Function class
  py::class_<Function>(m, "Function").def(py::init<const function_t>(), py::arg("fc"));

  // TaskR's Task class
  py::class_<Task>(m, "Task")
    .def(py::init<Function *, const workerId_t>(), py::arg("taskfc"), py::arg("workerAffinity") = -1)
    .def(py::init<const taskId_t, Function *, const workerId_t>(), py::arg("taskId"), py::arg("taskfc"), py::arg("workerAffinity") = -1)
    .def("getTaskId", &Task::getTaskId)
    .def("setTaskId", &Task::setTaskId)
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
    .def("notifyOne", &ConditionVariable::notifyOne)
    .def("notifyAll", &ConditionVariable::notifyAll)
    .def("getWaitingTaskCount", &ConditionVariable::getWaitingTaskCount);
}

} // namespace taskr