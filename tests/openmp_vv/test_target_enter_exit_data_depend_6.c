#pragma omp target enter data map(alloc: h_array[0:1000]) map(to: in_1[0:1000]) map(to: in_2[0:1000]) depend(out: h_array) depend(in: in_1) depend(in: in_2)
