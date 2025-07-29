MatMul
============

A simple example of pyTaskR registering a cpp-based function (in this case a Matrix-Matrix multiplication) to run in python.

- `main.py`: The main script to initialize TaskR runtime
- `matmul.cpp`: The cpp-function to be registered and callable in python
- `matmul.py`: The Driver to load the cpp function and execute it in python. It also consists of a NumPy example for comparisons.
