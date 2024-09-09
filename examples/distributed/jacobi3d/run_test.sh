echo "mpirun -n 88 mpi/jacobi -n 704 -px 1 -py 2 -pz 44 -i 30"
echo "mpirun -n 2 mate/jacobi --mate-ranks 44 -n 704 -px 1 -py 2 -pz 1 -lx 1 -ly 1 -lz 22 -i 30"
echo "mpirun -n 2 mate/jacobi --mate-ranks 88 -n 704 -px 1 -py 2 -pz 1 -lx 1 -ly 2 -lz 22 -i 30"
echo "mpirun -n 2 mate/jacobi --mate-ranks 176 -n 704 -px 1 -py 2 -pz 1 -lx 1 -ly 4 -lz 22 -i 5"
echo "mpirun -n 2 -bind-to socket -env OMP_NUM_THREADS 44 -env OMP_PLACES=threads taskr/jacobi -n 704 -px 1 -py 2 -pz 1 -lx 1 -ly 1 -lz 22 -i 30"
echo "mpirun -n 2 taskr/jacobi -n 704 -px 1 -py 2 -pz 1 -lx 1 -ly 1 -lz 22 -i 30"

# For debugging
# mpirun -n 4 xterm -e gdb -ex run --args mate/jacobi  --mate-ranks 176 -n 704 -px 1 -py 2 -pz 2 -lx 1 -ly 2 -lz 22 -i 50
 
