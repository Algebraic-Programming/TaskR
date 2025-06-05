import taskr

def simple(runtime):
  # Initializing taskr
  runtime.initialize()

  # fc = lambda task : print(f"Hello, I am task {task.getLabel()}")

  def fc(task):
    print(f"Before suspend task {task.getLabel()}")
    task.suspend()
    print(f"After suspend task {task.getLabel()}")

  # Create the taskr Tasks
  taskfc = taskr.Function(fc)

  # Creating the execution units (functions that the tasks will run)
  for i in range(2):
    task = taskr.Task(i, taskfc)

    # Adding to taskr
    runtime.addTask(task)

  # Running taskr for the current repetition
  runtime.run()

  # Waiting current repetition to end
  runtime.await_()

  # Finalizing taskr
  runtime.finalize()