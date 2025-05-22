import taskr

def simple(runtime):
  # Setting onTaskFinish callback to free up task memory when it finishes (not sure if we will have this)
  #runtime.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; })

  # Initializing taskr
  runtime.initialize

  fc = lambda task : print(f"Hello, I am task {task.getLabel()}")

  # Create the taskr Tasks
  taskfc = taskr.Function(fc)

  # Creating the execution units (functions that the tasks will run)
  for i in range(1):
    task = taskr.Task(i, taskfc)

    # Adding to taskr
    runtime.addTask = task

  # Running taskr for the current repetition
  runtime.run

  # Waiting current repetition to end
  runtime.await_

  # Finalizing taskr
  runtime.finalize