#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
#pragma omp target teams map(from: counts_atomic)
#pragma omp parallel
#pragma omp for
#pragma omp atomic
#pragma omp target teams map(from: counts_reduction)
#pragma omp parallel
#pragma omp for reduction(+: counts_team)
