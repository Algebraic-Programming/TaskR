CFLAGS = -O3 -g -Wfatal-errors 
LIBS = 
MPICXX = mpicxx

.SECONDARY: 
BINARIES = jacobi

.PHONY: all stage
all: $(BINARIES)

jacobi: jacobi.cpp grid.cpp
	$(MPICXX) $(CFLAGS) $(LIBS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(BINARIES) *.o 


	
 
