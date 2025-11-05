#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp declare target link(aint)
#pragma omp target map(from: x) map(to:y, z, aint)
#pragma omp target map (from: _ompvv_isOffloadingOn)
