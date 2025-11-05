#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp parallel shared(test_int)
#pragma omp scope private(test_int)
#pragma omp target map (from: _ompvv_isOffloadingOn)
