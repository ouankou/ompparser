#pragma omp target data map(from: h_matrix[dev*1000:1000]) device(dev)
