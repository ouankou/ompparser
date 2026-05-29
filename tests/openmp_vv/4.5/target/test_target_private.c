#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp parallel
#pragma omp target map(tofrom:compute_array[fp_val][0:10]) firstprivate(fp_val) private(p_val)
