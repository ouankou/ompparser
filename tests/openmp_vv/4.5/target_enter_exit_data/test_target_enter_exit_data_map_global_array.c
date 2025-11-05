#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data map(to: A)
#pragma omp target
#pragma omp target exit data map(from: A)
#pragma omp target data map(tofrom: A) map(from: B)
#pragma omp target exit data map(delete: A)
#pragma omp target map(to: A)
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target map (from: _ompvv_isOffloadingOn) map(to: _ompvv_isSharedEnv)
