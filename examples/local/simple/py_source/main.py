import taskr
import simple

def main():
    # maybe get resources as well as initializing number of PUs

    # 
    runtime = taskr.Runtime(ComputeManager, ComputeManager, [])

    # Running simple example
    simple(runtime)

if __name__ == "__main__":
    main()