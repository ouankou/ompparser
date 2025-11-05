#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
#pragma omp target map(from: version_set[0:2])
