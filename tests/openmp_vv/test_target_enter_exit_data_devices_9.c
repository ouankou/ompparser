#pragma omp target exit data map(from: h_matrix[dev][0 : 1000]) device(dev)
