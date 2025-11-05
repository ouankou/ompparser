#pragma omp target data map(to: h_array_h[0:1000]) map(from: h_array2_h[0:1000])
