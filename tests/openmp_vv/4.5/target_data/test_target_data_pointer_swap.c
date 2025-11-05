#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(tofrom: a[0:1024]) map(to: b[0:1024])
#pragma omp target map(alloc: a[0:1024], b[0:1024])
#pragma omp target data map(tofrom: a[0:1024]) map(to: b[0:1024])
#pragma omp target map(alloc: a[0:1024], b[0:1024])
#pragma omp target update from(a[0:1024])
#pragma omp target map (from: _ompvv_isOffloadingOn)
