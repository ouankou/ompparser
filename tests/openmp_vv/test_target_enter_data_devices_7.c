#pragma omp target exit data map(delete: h_matrix[dev][0:1000]) device(dev)
