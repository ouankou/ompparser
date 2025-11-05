#pragma omp target map(tofrom:compute_array[fp_val][0:10]) firstprivate(fp_val) private(p_val)
