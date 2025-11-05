#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(tofrom: a)
#pragma omp interchange
#pragma omp target map (from: _ompvv_isOffloadingOn)
