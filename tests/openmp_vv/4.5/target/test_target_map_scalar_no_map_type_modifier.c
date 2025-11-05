#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(from: compute_array) map(asclr)
#pragma omp target map(new_scalar)
#pragma omp target map (from: _ompvv_isOffloadingOn)
