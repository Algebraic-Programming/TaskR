CFLAGS = -O3 -g -Wfatal-errors -I.
LIBS =  -lmate
MPICXX = mpicxx

.SECONDARY: 
BINARIES = jacobi

.PHONY: all stage
all: $(BINARIES)

jacobi: mate_grid.cpp mate_jacobi.cpp
	$(MPICXX) $(CFLAGS) $(LIBS) -o $@ $^

mate_%.cpp: %.cpp
	mate-translator $< 

%.o: %.cpp
	$(MPICXX) -c $(CFLAGS) $< -Wno-unknown-pragmas

.PHONY: clean
clean:
	$(RM) $(BINARIES) *.o mate_*.cpp 


	
 
