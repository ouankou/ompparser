#pragma omp target update from(h_matrix[0:1000]) device(dev)
