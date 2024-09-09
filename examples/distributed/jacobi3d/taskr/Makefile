CFLAGS = -O3 -g -Wfatal-errors -I. -fopenmp 
#CFLAGS = -O0 -g -Wfatal-errors -I. -fopenmp
LIBS = -lboost_context
MPICXX = mpicxx

.SECONDARY: 
BINARIES = jacobi

.PHONY: all stage
all: $(BINARIES)

jacobi: grid.cpp jacobi.cpp
	$(MPICXX) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(MPICXX) -c $(CFLAGS) $< -Wno-unknown-pragmas

.PHONY: clean
clean:
	$(RM) $(BINARIES) *.o mate_*.cpp 


	
 
