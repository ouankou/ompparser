#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp assume no_openmp_routines
#pragma omp target parallel for map(tofrom: arr)
#pragma omp target map (from: _ompvv_isOffloadingOn)
