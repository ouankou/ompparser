#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp begin declare target
#pragma omp end declare target
#pragma omp target map(tofrom: test_val)
#pragma omp target map (from: _ompvv_isOffloadingOn)
