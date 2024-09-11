.SECONDARY: 
BINARIES = mpi/jacobi

.PHONY: all stage
all: $(BINARIES)

mpi/jacobi: 
	$(MAKE) -C mpi

.PHONY: clean
clean:
	$(MAKE) -C mpi clean 
	$(MAKE) -C mate clean


	
 
