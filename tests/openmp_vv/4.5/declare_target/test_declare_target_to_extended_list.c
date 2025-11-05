#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target to(aint)
#pragma omp declare target to(compute_array)
#pragma omp target map(from: x) map(to:y, z)
#pragma omp target map (from: _ompvv_isOffloadingOn)
