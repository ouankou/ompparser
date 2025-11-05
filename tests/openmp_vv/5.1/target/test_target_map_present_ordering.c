#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target data map(tofrom: x)
#pragma omp target map(present, to: x) map(from: x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
