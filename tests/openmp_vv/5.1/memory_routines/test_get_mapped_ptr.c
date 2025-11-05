#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target enter data device(i) map(to:x)
#pragma omp target exit data map(from:x)
#pragma omp target map (from: _ompvv_isOffloadingOn)
