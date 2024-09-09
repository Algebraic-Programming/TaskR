.SECONDARY: 
BINARIES = mpi/jacobi mate/jacobi

.PHONY: all stage
all: $(BINARIES)

mpi/jacobi: 
	$(MAKE) -C mpi

mate/jacobi: 
	$(MAKE) -C mate

.PHONY: clean
clean:
	$(MAKE) -C mpi clean 
	$(MAKE) -C mate clean


	
 
