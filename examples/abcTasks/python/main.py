import taskr
import abcTasks

def main():

    # Initialize taskr with the wanted compute manager backend and number of PUs
    t = taskr.create(backend="threading", num_workers=2)

    # Running simple example
    abcTasks.abcTasks(t)

if __name__ == "__main__":
    main()