#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target parallel map(tofrom: errors)
#pragma omp scope firstprivate(test_int)
#pragma omp target map (from: _ompvv_isOffloadingOn)
