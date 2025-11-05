#pragma omp target map(alloc: h_matrix[dev*1000:1000])
