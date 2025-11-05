#pragma omp target map(from: h_array[0:hsize]) map(alloc: help_sum, hsize) map(from:h_sum)
