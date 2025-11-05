#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map (from: _ompvv_isOffloadingOn)
#pragma omp target data map(to: a[:1024], b[:1024]) map(from: c)
#pragma omp target
#pragma omp target update to(b[:1024])
#pragma omp target
