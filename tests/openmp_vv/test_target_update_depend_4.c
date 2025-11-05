#pragma omp target enter data map(alloc: h_array[0:1000], in_1[0:1000], in_2[0:1000]) depend(out: h_array, in_1, in_2)
