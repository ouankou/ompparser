#pragma omp target map(tofrom:compute_array[p_val:1][0:10]) firstprivate(p_val)
