#pragma omp target enter data map(alloc: h_matrix[dev][0 : 1000]) device(dev)
