#pragma omp begin declare variant match(device={kind(host)})
#pragma omp end declare variant
#pragma omp begin declare variant match(device={kind(nohost)})
#pragma omp end declare variant
#pragma omp target map(to: b[0:1024], c[0:1024]) map(tofrom: a[0:1024])
#pragma omp simd
#pragma omp target map (from: _ompvv_isOffloadingOn)
