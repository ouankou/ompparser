#pragma omp target enter data map(alloc: h_array[0:1000]) depend(out: h_array)
