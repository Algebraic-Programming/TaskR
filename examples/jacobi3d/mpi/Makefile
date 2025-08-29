CXX = g++
CXXFLAGS = -O3 -g -Wfatal-errors
INCLUDES = -I/usr/local/include
LIBS = -L/usr/local/lib -lmpi

.SECONDARY:
BINARIES = jacobi

.PHONY: all clean

all: $(BINARIES)

jacobi: jacobi.cpp grid.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LIBS)

clean:
	$(RM) $(BINARIES) *.o
