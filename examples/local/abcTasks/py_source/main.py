import taskr
import abcTasks

def main():

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr("threading", 2)

    # Get the runtime
    runtime = t.get_runtime()

    # Running simple example
    abcTasks.abcTasks(runtime)

if __name__ == "__main__":
    main()