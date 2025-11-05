#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp parallel private(i)
#pragma omp target map(tofrom:compute_array[p_val:1][0:10]) firstprivate(p_val)
