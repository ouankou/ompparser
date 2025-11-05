#pragma omp target exit data map(from: h_array[0:1000]) depend(inout: h_array)
