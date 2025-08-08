import taskr

REPETITIONS = 5
ITERATIONS = 100

def abcTasks(runtime):

   # Create the taskr Tasks
  taskAfc = taskr.Function(lambda task : print(f"Task A {task.getTaskId()}"))
  taskBfc = taskr.Function(lambda task : print(f"Task B {task.getTaskId()}"))
  taskCfc = taskr.Function(lambda task : print(f"Task C {task.getTaskId()}"))

  # Initializing taskr
  runtime.initialize()

  # Our connection with the previous iteration is the last task C, null in the first iteration
  prevTaskC = taskr.Task(0, taskCfc)

  # Creating the execution units (functions that the tasks will run)
  for r in range(REPETITIONS):
    # Calculating the base task id for this repetition
    repetitionTaskId = r * ITERATIONS * 3

    for i in range(ITERATIONS):

      taskA = taskr.Task(repetitionTaskId + i * 3 + 0, taskAfc)
      taskB = taskr.Task(repetitionTaskId + i * 3 + 1, taskBfc)
      taskC = taskr.Task(repetitionTaskId + i * 3 + 2, taskCfc)

      # Creating dependencies
      if i > 0: taskA.addDependency(prevTaskC)
      taskB.addDependency(taskA)
      taskC.addDependency(taskB)

      # Adding to taskr runtime
      runtime.addTask(taskA)
      runtime.addTask(taskB)
      runtime.addTask(taskC)

      # Refreshing previous task C
      prevTaskC = taskC

    # Running taskr for the current repetition
    runtime.run()

    # Waiting current repetition to end
    runtime.wait()

  # Finalizing taskr
  runtime.finalize()