#pragma omp target map(alloc: h_array[0:1000]) map(from: h_array_copy[0:1000]) depend(in: h_array) depend(out: h_array_copy)
