import taskr

def simple(runtime):
  # TODO: Setting onTaskFinish callback to free up task memory when it finishes (not sure if we will have this)
  #runtime.setTaskCallbackHandler(HiCR::tasking::Task::callback_t::onTaskFinish, [&taskr](taskr::Task *task) { delete task; })

  print("runtime.initialize()", flush=True)

  # Initializing taskr
  runtime.initialize()

  fc = lambda task : print(f"Hello, I am task {task.getLabel()}")

  print("taskr.Function(fc)", flush=True)

  # Create the taskr Tasks
  taskfc = taskr.Function(fc)

  # Creating the execution units (functions that the tasks will run)
  for i in range(10):
    print("create task", flush=True)

    task = taskr.Task(i, taskfc)

    print("add task", flush=True)

    # Adding to taskr
    runtime.addTask(task)

  print("runtime.run()", flush=True)

  # Running taskr for the current repetition
  runtime.run()

  print("runtime.await_()", flush=True)

  # Waiting current repetition to end
  runtime.await_()

  print("runtime.finalize()", flush=True)

  # Finalizing taskr
  runtime.finalize()