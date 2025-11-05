#pragma omp target map(alloc: h_matrix[dev][0 : 1000]) device(dev)
