#pragma omp target exit data map(from: h_array[0:1000]) depend(in: h_array)
