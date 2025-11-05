#pragma omp target map(from: h_matrix_copy[dev][0:1000]) map(alloc: h_matrix[dev][0:1000])
