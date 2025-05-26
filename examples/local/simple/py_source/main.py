import taskr
import simple

def main():

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.taskr("threading", 1)

    # Get the runtime
    runtime = t.get_runtime()

    # Running simple example
    simple.simple(runtime)

if __name__ == "__main__":
    main()