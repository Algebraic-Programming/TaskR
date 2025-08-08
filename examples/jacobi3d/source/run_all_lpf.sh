#!/bin/bash

# Output file
OUTPUT_FILE="output_lpf.log"
echo "Starting batch run at $(date)" > "$OUTPUT_FILE"

# Values for each parameter
p_values=(1 2 4)

l_values=(1 2)

# Loop over all combinations
for b in "${p_values[@]}"; do
  for c in "${p_values[@]}"; do
    for d in "${p_values[@]}"; do
      a=$((b * c * d))
      for e in "${l_values[@]}"; do
        for f in "${l_values[@]}"; do
          for g in "${l_values[@]}"; do
            echo "Running with: a=$a, b=$b, c=$c, d=$d, e=$e, f=$f, g=$g" | tee -a "$OUTPUT_FILE"
            
            timeout 30 lpfrun  -n "$a" -engine zero ../../../build/examples/jacobi3d/threading -px "$b" -py "$c" -pz "$d" -lx "$e" -ly "$f" -lz "$g" -n 64 -i 10 \
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
