#!/bin/bash

# Output file
OUTPUT_FILE="output_mpi.log"
echo "Starting batch run at $(date)" > "$OUTPUT_FILE"

# Values for each parameter
values=(1 2 4)

# Loop over all combinations
for b in "${values[@]}"; do
  for c in "${values[@]}"; do
    for d in "${values[@]}"; do
      a=$((b * c * d))
      for e in "${values[@]}"; do
        for f in "${values[@]}"; do
          for g in "${values[@]}"; do
            echo "Running with: a=$a, b=$b, c=$c, d=$d, e=$e, f=$f, g=$g" | tee -a "$OUTPUT_FILE"

            timeout 30 mpirun -n "$a" --oversubscribe ../../../build/examples/jacobi3d/threading -px "$b" -py "$c" -pz "$d" -lx "$e" -ly "$f" -lz "$g" -n 64 -i 10 \
              >> "$OUTPUT_FILE" 2>&1
            
            exit_status=$?
            if [ $exit_status -eq 124 ]; then
              echo "Run timed out after 30s for: a=$a, b=$b, c=$c, d=$d, e=$e, f=$f, g=$g" | tee -a "$OUTPUT_FILE"
            elif [ $exit_status -ne 0 ]; then
              echo "Command failed for: a=$a, b=$b, c=$c, d=$d, e=$e, f=$f, g=$g (exit code $exit_status)" | tee -a "$OUTPUT_FILE"
            fi
          done
        done
      done
    done
  done
done

echo "Batch run finished at $(date)" >> "$OUTPUT_FILE"
